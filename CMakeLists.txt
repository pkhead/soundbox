cmake_minimum_required(VERSION 2.8.12)
project(soundbox)

set(CMAKE_CXX_STANDARD 11)

find_library(GLFW3_LIB NAMES glfw3)
find_path(GLFW3_INCLUDE NAMES GLFW/glfw3.h)

if (NOT GLFW3_LIB)
    message(FATAL_ERROR "Could not find library glfw3")
endif()

if (NOT GLFW3_INCLUDE)
    message(FATAL_ERROR "Could not find GLFW/glfw3.h")
endif()

add_executable(soundbox
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_opengl3.cpp

    src/main.cpp
    src/song.cpp
    src/audio.cpp
    src/ui.cpp
)

target_include_directories(soundbox PUBLIC ${GLFW3_INCLUDE} imgui libsoundio/soundio)

# todo: add libsoundio for windows
if (WIN32)
    find_library(OPENGL_FOUND NAMES opengl32)

    if (NOT OPENGL_FOUND)
        message(FATAL_ERROR "Could not find library opengl32")
    endif()

    file(COPY ${CMAKE_SOURCE_DIR}/libsoundio/build/soundio.dll DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    target_link_directories(soundbox PUBLIC build)
    target_link_libraries(soundbox ${GLFW3_LIB} ${OPENGL_FOUND} ${CMAKE_SOURCE_DIR}/libsoundio/build/soundio.lib)
else()
    find_library(OPENGL_FOUND NAMES GL)
    find_library(X11_FOUND NAMES X11)

    if (NOT OPENGL_FOUND)
        message(FATAL_ERROR "Could not find library GL")
    endif()

    if (NOT X11_FOUND)
        message(FATAL_ERROR "Could not find library X11")
    endif()

    target_link_directories(soundbox PUBLIC build)
    file(COPY libsoundio/build/libsoundio.so DESTINATION ${CMAKE_CURRENT_BINARY_DIR} FOLLOW_SYMLINK_CHAIN)
    target_link_libraries(soundbox glfw3 GL X11 soundio)

endif()