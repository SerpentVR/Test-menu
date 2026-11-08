// Native mod menu (ImGui) -> builds to libModMenu.so
// Needs: Dear ImGui sources (imgui/, with backends/imgui_impl_opengl3.*) and Dobby (dobby.h + libdobby.a)
#include <jni.h>
#include <pthread.h>
#include <unistd.h>
#include <android/log.h>
#include <android/input.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
   #include <dlfcn.h>
   #include "And64InlineHook.hpp"

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "ModMenu", __VA_ARGS__)

// ---- Menu state: read these from your game hooks ----
namespace Cfg {
    bool godMode = false, infAmmo = false, noRecoil = false, esp = false, noclip = false;
    float speed = 1.0f, jump = 1.0f, fov = 90.0f;
    bool open = false;
}

static bool g_init = false;
static int g_w = 0, g_h = 0;
static float g_touchX = 0, g_touchY = 0;
static bool g_touchDown = false;

// ---- Menu drawing ----
static void DrawMenu() {
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);

    if (!Cfg::open) {                       // collapsed: small floating button
        ImGui::SetNextWindowSize(ImVec2(0, 0));
        ImGui::Begin("##icon", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize);
        if (ImGui::Button("MOD", ImVec2(110, 90))) Cfg::open = true;
        ImGui::End();
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(g_w * 0.42f, g_h * 0.65f), ImGuiCond_FirstUseEver);
    ImGui::Begin("GTH MOD MENU", &Cfg::open);
    if (ImGui::BeginTabBar("tabs")) {
        if (ImGui::BeginTabItem("Player")) {
            ImGui::Checkbox("God Mode", &Cfg::godMode);
            ImGui::Checkbox("Infinite Ammo", &Cfg::infAmmo);
            ImGui::Checkbox("No Recoil", &Cfg::noRecoil);
            ImGui::SliderFloat("Speed", &Cfg::speed, 1.f, 5.f);
            ImGui::SliderFloat("Jump", &Cfg::jump, 1.f, 5.f);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Visual")) {
            ImGui::Checkbox("ESP Boxes", &Cfg::esp);
            ImGui::SliderFloat("FOV", &Cfg::fov, 60.f, 120.f);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Misc")) {
            ImGui::Checkbox("Noclip", &Cfg::noclip);
            if (ImGui::Button("Reset All", ImVec2(-1, 80))) {
                Cfg::godMode = Cfg::infAmmo = Cfg::noRecoil = Cfg::esp = Cfg::noclip = false;
                Cfg::speed = Cfg::jump = 1.f; Cfg::fov = 90.f;
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

// ---- Render hook (draws the menu on top of the game every frame) ----
static EGLBoolean (*old_eglSwapBuffers)(EGLDisplay, EGLSurface) = nullptr;

static EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surf) {
    eglQuerySurface(dpy, surf, EGL_WIDTH, &g_w);
    eglQuerySurface(dpy, surf, EGL_HEIGHT, &g_h);
    if (g_w < 100 || g_h < 100) return old_eglSwapBuffers(dpy, surf);

    if (!g_init) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        ImGui::StyleColorsDark();
        ImGui::GetStyle().ScaleAllSizes(3.0f);          // bigger for touchscreens
        io.FontGlobalScale = 3.0f;
        ImGui_ImplOpenGL3_Init("#version 300 es");
        g_init = true;
        LOG("ImGui ready %dx%d", g_w, g_h);
    }

    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float) g_w, (float) g_h);
    io.AddMousePosEvent(g_touchX, g_touchY);
    io.AddMouseButtonEvent(0, g_touchDown);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    DrawMenu();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    return old_eglSwapBuffers(dpy, surf);
}

// ---- Touch input hook ----
// Works for games that read input through the NDK input queue.
// Unity/other engines may route touches differently (see notes).
static int32_t (*old_getEvent)(AInputQueue *, AInputEvent **) = nullptr;

static int32_t hook_getEvent(AInputQueue *q, AInputEvent **ev) {
    int32_t r = old_getEvent(q, ev);
    if (r >= 0 && ev && *ev && AInputEvent_getType(*ev) == AINPUT_EVENT_TYPE_MOTION) {
        int32_t act = AMotionEvent_getAction(*ev) & AMOTION_EVENT_ACTION_MASK;
        g_touchX = AMotionEvent_getX(*ev, 0);
        g_touchY = AMotionEvent_getY(*ev, 0);
        g_touchDown = (act == AMOTION_EVENT_ACTION_DOWN || act == AMOTION_EVENT_ACTION_MOVE);
    }
    return r;
}

// ---- Entry point: runs when the game loads the library ----
struct AnyPtr {
    void *p;
    template <class T> operator T() const { return (T) p; }
};

static void *MainThread(void *) {
    sleep(5);   // let the game finish starting up
    void *egl = DobbySymbolResolver("libEGL.so", "eglSwapBuffers");
    if (egl) DobbyHook(egl, AnyPtr{(void *) hook_eglSwapBuffers}, AnyPtr{(void *) &old_eglSwapBuffers});
    void *inp = DobbySymbolResolver("libandroid.so", "AInputQueue_getEvent");
    if (inp) DobbyHook(inp, AnyPtr{(void *) hook_getEvent}, AnyPtr{(void *) &old_getEvent});
    LOG("hooks installed (egl=%p input=%p)", egl, inp);
    return nullptr;
}

__attribute__((constructor))
static void OnLoad() {
    pthread_t t;
    pthread_create(&t, nullptr, MainThread, nullptr);
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *, void *) { return JNI_VERSION_1_6; }
