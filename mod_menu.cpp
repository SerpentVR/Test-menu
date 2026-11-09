// Runtime IL2CPP dumper -> libModMenu.so
// When the game runs, this writes il2cpp_dump.txt (class, field and method names + addresses)
// into the game's own files folder. No PC needed.
#include <pthread.h>
#include <unistd.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <string>
#include <android/log.h>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "ModDump", __VA_ARGS__)
#define S(x) ((x) ? (x) : "")

typedef void *(*fn_domain_get)();
typedef void *(*fn_thread_attach)(void *);
typedef void **(*fn_get_assemblies)(void *, size_t *);
typedef void *(*fn_asm_image)(void *);
typedef const char *(*fn_name_of)(void *);
typedef size_t (*fn_class_count)(void *);
typedef void *(*fn_image_class)(void *, size_t);
typedef void *(*fn_iter)(void *, void **);
typedef uint32_t (*fn_param_count)(void *);
typedef size_t (*fn_field_offset)(void *);

static void mkdirs(const std::string &p) {
    for (size_t i = 1; i < p.size(); i++)
        if (p[i] == '/') mkdir(p.substr(0, i).c_str(), 0777);
    mkdir(p.c_str(), 0777);
}

static std::string pkgName() {
    char buf[256] = {0};
    FILE *f = fopen("/proc/self/cmdline", "r");
    if (f) { size_t n = fread(buf, 1, sizeof(buf) - 1, f); (void) n; fclose(f); }
    return std::string(buf);
}

static void *DumpThread(void *) {
    // Wait for the game to load its il2cpp library
    void *lib = nullptr;
    for (int i = 0; i < 300 && !lib; i++) {
        lib = dlopen("libil2cpp.so", RTLD_NOW | RTLD_NOLOAD);
        if (!lib) sleep(1);
    }
    if (!lib) { LOG("libil2cpp.so not found"); return nullptr; }
    sleep(25);  // let the game finish starting up

    auto domain_get      = (fn_domain_get)      dlsym(lib, "il2cpp_domain_get");
    auto thread_attach   = (fn_thread_attach)   dlsym(lib, "il2cpp_thread_attach");
    auto get_assemblies  = (fn_get_assemblies)  dlsym(lib, "il2cpp_domain_get_assemblies");
    auto asm_image       = (fn_asm_image)       dlsym(lib, "il2cpp_assembly_get_image");
    auto image_name      = (fn_name_of)         dlsym(lib, "il2cpp_image_get_name");
    auto image_classes   = (fn_class_count)     dlsym(lib, "il2cpp_image_get_class_count");
    auto image_class     = (fn_image_class)     dlsym(lib, "il2cpp_image_get_class");
    auto class_name      = (fn_name_of)         dlsym(lib, "il2cpp_class_get_name");
    auto class_ns        = (fn_name_of)         dlsym(lib, "il2cpp_class_get_namespace");
    auto class_methods   = (fn_iter)            dlsym(lib, "il2cpp_class_get_methods");
    auto class_fields    = (fn_iter)            dlsym(lib, "il2cpp_class_get_fields");
    auto method_name     = (fn_name_of)         dlsym(lib, "il2cpp_method_get_name");
    auto method_params   = (fn_param_count)     dlsym(lib, "il2cpp_method_get_param_count");
    auto field_name      = (fn_name_of)         dlsym(lib, "il2cpp_field_get_name");
    auto field_offset    = (fn_field_offset)    dlsym(lib, "il2cpp_field_get_offset");

    if (!domain_get || !thread_attach || !get_assemblies || !asm_image || !image_name ||
        !image_classes || !image_class || !class_name || !class_ns || !class_methods ||
        !class_fields || !method_name || !method_params || !field_name || !field_offset) {
        LOG("some il2cpp functions are missing");
        return nullptr;
    }

    void *domain = domain_get();
    thread_attach(domain);

    Dl_info info;
    dladdr((void *) domain_get, &info);
    uintptr_t base = (uintptr_t) info.dli_fbase;

    std::string pkg = pkgName();
    std::string dir = "/sdcard/Android/data/" + pkg + "/files";
    mkdirs(dir);
    std::string path = dir + "/il2cpp_dump.txt";
    FILE *out = fopen(path.c_str(), "w");
    if (!out) {
        dir = "/data/data/" + pkg + "/files";
        mkdirs(dir);
        path = dir + "/il2cpp_dump.txt";
        out = fopen(path.c_str(), "w");
    }
    if (!out) { LOG("cannot open output file"); return nullptr; }
    LOG("writing dump to %s", path.c_str());

    size_t asmCount = 0;
    void **assemblies = get_assemblies(domain, &asmCount);
    for (size_t a = 0; a < asmCount; a++) {
        void *image = asm_image(assemblies[a]);
        const char *iname = S(image_name(image));
        if (!strstr(iname, "Assembly-CSharp")) continue;   // only the game's own code
        fprintf(out, "// ===== %s =====\n", iname);

        size_t cc = image_classes(image);
        for (size_t c = 0; c < cc; c++) {
            void *k = image_class(image, c);
            fprintf(out, "\nclass %s.%s\n", S(class_ns(k)), S(class_name(k)));

            void *it = nullptr;
            void *f;
            while ((f = class_fields(k, &it)))
                fprintf(out, "  field %s  offset=0x%zx\n", S(field_name(f)), field_offset(f));

            it = nullptr;
            void *m;
            while ((m = class_methods(k, &it))) {
                void *fp = *(void **) m;   // first field of a MethodInfo is the code address
                size_t rva = fp ? (size_t) ((uintptr_t) fp - base) : 0;
                fprintf(out, "  method %s(%u)  rva=0x%zx\n", S(method_name(m)), method_params(m), rva);
            }
        }
        fflush(out);
    }
    fprintf(out, "\n// DONE\n");
    fclose(out);
    LOG("dump finished");
    return nullptr;
}

__attribute__((constructor))
static void OnLoad() {
    pthread_t t;
    pthread_create(&t, nullptr, DumpThread, nullptr);
}
