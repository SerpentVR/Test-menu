cmake_minimum_required(VERSION 3.10)
project(ModMenu)

set(CMAKE_CXX_STANDARD 17)

# imgui/ and dobby_src/ are downloaded by the GitHub workflow (or clone them yourself)
set(IMGUI ${CMAKE_SOURCE_DIR}/imgui)
set(DOBBY_GENERATE_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/dobby_src dobby_build)

include_directories(${IMGUI} ${CMAKE_SOURCE_DIR}/dobby_src/include)

add_library(ModMenu SHARED
    mod_menu.cpp
    ${IMGUI}/imgui.cpp
    ${IMGUI}/imgui_draw.cpp
    ${IMGUI}/imgui_tables.cpp
    ${IMGUI}/imgui_widgets.cpp
    ${IMGUI}/backends/imgui_impl_opengl3.cpp)

target_link_libraries(ModMenu dobby log android EGL GLESv3)
