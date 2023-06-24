cmake_minimum_required(VERSION 3.11)
project(soundbox)

set(CMAKE_CXX_STANDARD 17)

# send these options to glfw
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(deps/portaudio-19.7.0 EXCLUDE_FROM_ALL)
add_subdirectory(deps/glfw)

mark_as_advanced(GLFW_BUILD_DOCS)
mark_as_advanced(GLFW_BUILD_TESTS)
mark_as_advanced(GLFW_BUILD_EXAMPLES)

set(SOURCES
    # imgui sources
    deps/imgui/imgui_demo.cpp
    deps/imgui/imgui_draw.cpp
    deps/imgui/imgui_tables.cpp
    deps/imgui/imgui_widgets.cpp
    deps/imgui/imgui.cpp
    deps/imgui/backends/imgui_impl_glfw.cpp
    deps/imgui/backends/imgui_impl_opengl3.cpp

    # native file dialog
    deps/nativefiledialog/src/nfd_common.c

    # anamark tuning library
    deps/AnaMark-Tuning-Library/TUN_Scale.cpp
    deps/AnaMark-Tuning-Library/TUN_StringTools.cpp
    deps/AnaMark-Tuning-Library/SCL_Import.cpp

    # glad
    deps/gl/glad/gl.c

    # app sources
    src/main.cpp
    src/song.cpp
    src/serialize.cpp
    src/audio.cpp
    src/sys.cpp
    src/audiofile.cpp
    src/plugins.cpp
    src/plugin_hosts/ladspa.cpp
    
    # sources for app ui
    src/ui/ui.cpp
    src/ui/editor.cpp
    src/ui/change_history.cpp
    src/ui/theme.cpp

    src/ui/song_settings.cpp
    src/ui/channel_settings.cpp
    src/ui/pattern_editor.cpp
    src/ui/track_editor.cpp
    src/ui/fx_mixer.cpp

    src/ui/plugin_list.cpp
    src/ui/directories.cpp
    src/ui/tunings.cpp
    src/ui/themes_ui.cpp

    src/ui/imgui_stdlib.cpp

    # audio modules
    src/modules/modules.cpp
    src/modules/analyzer.cpp
    src/modules/gain.cpp
    src/modules/volume.cpp
    src/modules/waveform.cpp
    src/modules/delay.cpp
)

include_directories(
    deps
    deps/gl
    deps/AnaMark-Tuning-Library
    deps/glfw/include
    deps/imgui
    deps/portaudio-19.7.0/include
    deps/nativefiledialog/src/include
    deps/tuning-library/include
)

# Windows
if (WIN32)
    # append platform-specific files for nativefiledialog sources
    set(SOURCES ${SOURCES}
        deps/nativefiledialog/src/nfd_win.cpp
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

    target_link_libraries(soundbox glfw dwmapi comctl32.lib portaudio_static ${GLFW_LIBRARIES} ${OPENGL_LIBRARIES})

# Linux
else()
    # append platform-specific files for nativefiledialog sources
    set(SOURCES ${SOURCES}
        deps/nativefiledialog/src/nfd_zenity.c
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

    target_link_libraries(soundbox glfw portaudio_static ${GLFW_LIBRARIES} ${OPENGL_LIBRARIES} ${X11_LIBRARIES})
endif()

# add debug macro
target_compile_definitions(soundbox PUBLIC "$<$<CONFIG:DEBUG>:DEBUG>")

# copy directories to binary directory, mainly for easy use in Visual Studio
file(COPY ${CMAKE_SOURCE_DIR}/fonts DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/styles DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/imgui.ini DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/logo.png DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
