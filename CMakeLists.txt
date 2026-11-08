cmake_minimum_required(VERSION 3.10)
project(ModMenu)

set(CMAKE_CXX_STANDARD 17)

set(IMGUI ${CMAKE_SOURCE_DIR}/imgui)
set(HOOK ${CMAKE_SOURCE_DIR}/hooklib)

add_definitions(-DIMGUI_IMPL_OPENGL_ES3)
include_directories(${IMGUI} ${HOOK})

add_library(ModMenu SHARED
    mod_menu.cpp
    ${HOOK}/And64InlineHook.cpp
    ${IMGUI}/imgui.cpp
    ${IMGUI}/imgui_draw.cpp
    ${IMGUI}/imgui_tables.cpp
    ${IMGUI}/imgui_widgets.cpp
    ${IMGUI}/backends/imgui_impl_opengl3.cpp)

target_link_libraries(ModMenu log android EGL GLESv3)
