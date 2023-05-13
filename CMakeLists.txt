cmake_minimum_required(VERSION 2.8.12)
project(soundbox)

set(CMAKE_CXX_STANDARD 11)

add_library(imgui
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC imgui)

add_executable(soundbox
    src/main.cpp
    src/song.cpp
)

target_link_libraries(soundbox imgui)
target_link_libraries(soundbox glfw3 GL X11)