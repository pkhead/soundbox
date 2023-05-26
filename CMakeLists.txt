cmake_minimum_required(VERSION 3.11)
project(soundbox)

set(CMAKE_CXX_STANDARD 11)

mark_as_advanced(GLFW3_LIB)
mark_as_advanced(GLFW3_INCLUDE)
mark_as_advanced(NFD_LIBRARIES)

find_library(GLFW3_LIB NAMES glfw3)
find_path(GLFW3_INCLUDE NAMES GLFW/glfw3.h)
find_library(NFD_LIBRARIES PATH ${CMAKE_SOURCE_DIR}/nativefiledialog/build/lib/)

if (NOT GLFW3_LIB)
    message(FATAL_ERROR "Could not find library glfw3")
endif()

if (NOT GLFW3_INCLUDE)
    message(FATAL_ERROR "Could not find GLFW/glfw3.h")
endif()

set(SOURCES
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_opengl3.cpp

    nativefiledialog/src/nfd_common.c

    src/main.cpp
    src/song.cpp
    src/audio.cpp
    src/ui.cpp
    src/sys.cpp

    # modules
    src/modules/waveform.cpp
    src/modules/volume.cpp
)

include_directories(${GLFW3_INCLUDE} imgui libsoundio/soundio nativefiledialog/src/include)

if (WIN32)
    # append platform-specific files for nativefiledialog sources
    set(SOURCES ${SOURCES}
        nativefiledialog/src/nfd_win.cpp
    )

    add_executable(soundbox ${SOURCES})
    # target_include_directories(soundbox PUBLIC ${INCLUDE})

    find_package(OpenGL REQUIRED)

    if (NOT OpenGL_FOUND)
        message(FATAL_ERROR "Could not find library opengl32")
    endif()

    file(COPY ${CMAKE_SOURCE_DIR}/libsoundio/build/soundio.dll DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    target_link_directories(soundbox PUBLIC build)
    target_link_libraries(soundbox comctl32.lib ${GLFW3_LIB} ${OPENGL_LIBRARIES} ${CMAKE_SOURCE_DIR}/libsoundio/build/soundio.lib)
else()
    # append platform-specific files for nativefiledialog sources
    set(SOURCES ${SOURCES}
        nativefiledialog/src/nfd_zenity.c
    )

    add_executable(soundbox ${SOURCES})
    # target_include_directories(soundbox PUBLIC ${INCLUDE})

    find_package(OpenGL REQUIRED)
    find_package(X11 REQUIRED)

    if (NOT OPENGL_FOUND)
        message(FATAL_ERROR "Could not find library GL")
    endif()

    if (NOT X11_FOUND)
        message(FATAL_ERROR "Could not find library X11")
    endif()

    target_link_directories(soundbox PUBLIC build)
    file(COPY libsoundio/build/libsoundio.so DESTINATION ${CMAKE_CURRENT_BINARY_DIR} FOLLOW_SYMLINK_CHAIN)
    target_link_libraries(soundbox glfw3 ${OPENGL_LIBRARIES} ${X11_LIBRARIES} soundio)
endif()