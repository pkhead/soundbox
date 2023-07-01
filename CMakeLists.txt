cmake_minimum_required(VERSION 3.18)
project(soundbox)

set(CMAKE_CXX_STANDARD 17)

# set default build type
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

# find packages required for lv2 support on system
find_library(LILV_LIBRARIES lilv-0)
find_path(LILV_INCLUDE lilv/lilv.h PATH_SUFFIXES lilv-0)
find_library(SUIL_LIBRARIES suil-0)
find_path(SUIL_INCLUDE suil/suil.h PATH_SUFFIXES suil-0)

if (
    (LILV_LIBRARIES STREQUAL "LILV_LIBRARIES-NOTFOUND") OR
    (LILV_INCLUDE STREQUAL "LILV_INCLUDE-NOTFOUND") OR
    (SUIL_LIBRARIES STREQUAL "SUIL_LIBRARIES-NOTFOUND") OR
    (SUIL_INCLUDE STREQUAL "SUIL_INCLUDE-NOTFOUND")
)
    set(STATUS_LV2 "No")
else()
    set(STATUS_LV2 "Yes")
    set(LV2_FOUND 1)
endif()

# gtk2 for certain plugin uis
option(SBOX_DISABLE_GTK2 "Do not support GTK2 plugin UIs" OFF)
option(SBOX_DISABLE_COMPOSITE "Do not support embedded plugin UIs" OFF)

if(NOT SBOX_DISABLE_GTK2)
    find_package(GTK2)
endif()

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

    # tomlcpp
    deps/tomlcpp/toml.c
    deps/tomlcpp/tomlcpp.cpp

    # glad
    deps/gl/glad/gl.c

    # app sources
    src/main.cpp
    src/winmgr.cpp
    src/song.cpp
    src/serialize.cpp
    src/audio.cpp
    src/sys.cpp
    src/audiofile.cpp
    src/plugins.cpp
    src/worker.cpp
    src/util.cpp

    # lv2 plugins
    src/plugin_hosts/ladspa.cpp
    src/plugin_hosts/lv2-host/features.cpp
    src/plugin_hosts/lv2-host/host.cpp
    src/plugin_hosts/lv2-host/interface.cpp
    src/plugin_hosts/lv2-host/ui.cpp
    
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

set(LIBRARIES
    glfw
    portaudio_static
    ${LILV_LIBRARIES}
    ${SUIL_LIBRARIES}
    ${GTK2_LIBRARIES}
)

set(INCLUDES
    deps
    deps/gl
    deps/AnaMark-Tuning-Library
    deps/glfw/include
    deps/imgui
    deps/portaudio-19.7.0/include
    deps/nativefiledialog/src/include
    deps/tuning-library/include
)

set(COMPILE_FLAGS "")

if (LV2_FOUND)
    set(INCLUDES ${INCLUDES} ${LILV_INCLUDE} ${SUIL_INCLUDE})
    set(COMPILE_FLAGS ${COMPILE_FLAGS} ENABLE_LV2)
endif()
    
if (GTK2_FOUND)
    set(INCLUDES ${INCLUDES} ${GTK2_INCLUDE_DIRS})
    set(COMPILE_FLAGS ${COMPILE_FLAGS} ENABLE_GTK2)
endif()

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

    target_compile_definitions(soundbox PUBLIC UI_WINDOWS ${COMPILE_FLAGS})
    target_link_libraries(soundbox dwmapi comctl32.lib ${LIBRARIES} ${GLFW_LIBRARIES} ${OPENGL_LIBRARIES})
    set(STATUS_COMPOSITE "No")

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

    find_library(LIB_XFIXES Xfixes)
    find_library(LIB_XCOMPOSITE Xcomposite)

    if(SBOX_DISABLE_COMPOSITE OR LIB_XFIXES STREQUAL "LIB_XFIXES-NOTFOUND" OR LIB_XCOMPOSITE STREQUAL "LIB_XCOMPOSITE-NOTFOUND")
        set(STATUS_COMPOSITE "No")
        set(LIB_XFIXES "")
        set(LIB_XCOMPOSITE "")
    else()
        set(STATUS_COMPOSITE "Yes")
        target_compile_definitions(soundbox PUBLIC COMPOSITING)
    endif()

    target_compile_definitions(soundbox PUBLIC UI_X11 ${COMPILE_FLAGS})
    target_link_libraries(soundbox
        ${LIB_XFIXES}
        ${LIB_XCOMPOSITE}
        ${LIBRARIES}
        ${GLFW_LIBRARIES}
        ${OPENGL_LIBRARIES}
        ${X11_LIBRARIES}
    )
endif()

target_include_directories(soundbox PUBLIC ${INCLUDES})

# add debug macro
target_compile_definitions(soundbox PUBLIC "$<$<CONFIG:DEBUG>:DEBUG>")

# copy directories to binary directory, mainly for easy use in Visual Studio
file(COPY ${CMAKE_SOURCE_DIR}/fonts DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/styles DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/imgui.ini DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/logo.png DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# print build status
if(GTK2_FOUND)
    set(STATUS_GTK2 "Yes")
else()
    set(STATUS_GTK2 "No")
endif()

message("\n"
    "Configuration:\n"
    " - Build Type                  ${CMAKE_BUILD_TYPE}\n"
    " - LV2 plugins (lilv, suil)    ${STATUS_LV2}\n"
    " - GTK2 plugin UIs             ${STATUS_GTK2}\n"
    " - Plugin UI embedding         ${STATUS_COMPOSITE}\n"
    "\n"
)