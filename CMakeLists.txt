cmake_minimum_required(VERSION 3.11)
project(soundbox)

set(CMAKE_CXX_STANDARD 11)

# send these options to libsoundio
set(BUILD_STATIC_LIBS OFF CACHE BOOL "Build libsoundio static libraries" FORCE)
set(BUILD_EXAMPLE_PROGRAMS OFF CACHE BOOL "Build libsoundio example programs" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "Build libsoundio test programs" FORCE)

add_subdirectory(libsoundio)
add_subdirectory(glfw)

mark_as_advanced(BUILD_STATIC_LIBS)
mark_as_advanced(BUILD_EXAMPLE_PROGRAMS)
mark_as_advanced(BUILD_TESTS)

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
    src/audiofile.cpp

    # modules
    src/modules/create.cpp
    src/modules/waveform.cpp
    src/modules/volume.cpp
    src/modules/gain.cpp
    src/modules/analyzer.cpp
)

include_directories(glfw/include imgui libsoundio/soundio nativefiledialog/src/include)

# Windows
if (WIN32)
    # append platform-specific files for nativefiledialog sources
    set(SOURCES ${SOURCES}
        nativefiledialog/src/nfd_win.cpp
    )

    if (CMAKE_BUILD_TYPE MATCHES Debug)
      add_executable(soundbox ${SOURCES})
    else()
        # if not making a debug build, remove the console window
        add_executable(soundbox WIN32 ${SOURCES})
        target_compile_definitions(soundbox PUBLIC USE_WIN32_MAIN)
    endif()

    find_package(OpenGL REQUIRED)

    if (NOT OpenGL_FOUND)
        message(FATAL_ERROR "Could not find library opengl32")
    endif()

    target_link_libraries(soundbox glfw comctl32.lib ${GLFW_LIBRARIES} ${OPENGL_LIBRARIES} libsoundio_shared)

    # copy soundio.dll to the binary directory
    add_custom_command(TARGET soundbox POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/libsoundio/soundio.dll"
            $<TARGET_FILE_DIR:soundbox>
    )

# Linux
else()
    # append platform-specific files for nativefiledialog sources
    set(SOURCES ${SOURCES}
        nativefiledialog/src/nfd_zenity.c
    )

    add_executable(soundbox ${SOURCES})

    find_package(OpenGL REQUIRED)
    find_package(X11 REQUIRED)

    if (NOT OPENGL_FOUND)
        message(FATAL_ERROR "Could not find library GL")
    endif()

    if (NOT X11_FOUND)
        message(FATAL_ERROR "Could not find library X11")
    endif()

    target_link_libraries(soundbox glfw ${GLFW_LIBRARIES} ${OPENGL_LIBRARIES} ${X11_LIBRARIES} libsoundio_shared)
endif()

file(COPY ${CMAKE_SOURCE_DIR}/fonts DESTINATION ${CMAKE_CURRENT_BINARY_DIR})