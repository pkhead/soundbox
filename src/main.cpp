#include <cassert>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <sstream>

#include <glad/gl.h>
#include <imgui.h>
#include "editor/theme.h"
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <math.h>
#include "sys.h"

#ifdef ENABLE_LV2
#include "plugin_hosts/lv2-host/lv2interface.h"
#endif

#ifdef _WIN32
// i want the title bar to match light/dark theme in windows
// using the dwm api
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <dwmapi.h>
#include <wchar.h>

#ifndef INCLUDE_GLFW_NATIVE
#define INCLUDE_GLFW_NATIVE
#endif

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#endif

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "audiofile.h"
#include "ui/ui.h"
#include "song.h"
#include "editor/editor.h"
#include "audio.h"
#include "sys.h"
#include "util.h"
#include "winmgr.h"

bool IS_BIG_ENDIAN;

static void glfw_error_callback(int error, const char *description)
{
    std::cerr << "GLFW error " << error << ": " << description << "\n";
}

#ifdef USE_WIN32_MAIN
int main(int argc, char** argv);

int wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    int argc;
    LPWSTR* argv_w = CommandLineToArgvW(lpCmdLine, &argc);

    // convert wchar to char
    char** argv = new char*[argc];

    for (int i = 0; i < argc; i++)
    {
        char* str = new char[wcslen(argv_w[i])];
        argv[i] = str;
        WideCharToMultiByte(CP_UTF8, NULL, argv_w[i], -1, str, -1, NULL, NULL);
    }

    // call main
    int err = main(argc, argv);

    // deallocate memory
    for (int i = 0; i < argc; i++)
    {
        delete argv[i];
    }
    delete[] argv;

    // return error code
    return err;
}
#endif

#ifndef UNIT_TESTS
int main(int argc, char** argv)
{
    { // record endianness
        union {
            uint32_t i;
            char c[4];
        } static bint = {0x01020304};

        IS_BIG_ENDIAN = bint.c[0] == 1;
    }

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // OpenGL 3.3 + GLSL 330
    const char *glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // create root window
    WindowManager window_manager(1280, 720, "soundbox");
    // vsync is automatically enabled by winmgr
    GLFWwindow* root_window = window_manager.root_window();
    GLFWwindow* draw_window = window_manager.draw_window();
    glfwMakeContextCurrent(draw_window);

#ifdef _WIN32
    // match titlebar with user's theme
    {
        HWND win = glfwGetWin32Window(root_window);
        BOOL value = true;
        DwmSetWindowAttribute(win, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
    }
#endif

    // load glad
    gladLoadGL(glfwGetProcAddress);

    float screen_xscale, screen_yscale;
    glfwGetWindowContentScale(draw_window, &screen_xscale, &screen_yscale);

    // setup LV2 plugin host
#ifdef ENABLE_LV2
    lv2::lv2_init(&argc, &argv);
#endif

    // setup dear imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImFont* font_proggy = io.Fonts->AddFontFromFileTTF("fonts/ProggyClean.ttf", 13.0f * screen_yscale);
    ImFont* font_inconsolata = io.Fonts->AddFontFromFileTTF("fonts/Inconsolata-Regular.ttf", 13.0f * screen_yscale);
    ImGui::GetStyle().ScaleAllSizes(screen_yscale);

    // on high dpi monitors, proggy doesn't display nicely
    // so set the default font to inconsolata on such a monitor
    // (probably a weird decision to do this, but i like the proggy font)
    if (screen_yscale == 1.0f)  io.FontDefault = font_proggy;
    else                        io.FontDefault = font_inconsolata;

    ImGui_ImplGlfw_InitForOpenGL(draw_window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImGui::StyleColorsClassic();

    // setup audio backend
    AudioDevice::_pa_start();

    ui::show_demo_window = false;

    AudioDevice device(-1);

    {
        const size_t BUFFER_SIZE = 128;

        audiomod::ModuleContext modctx(device.sample_rate(), device.num_channels(), BUFFER_SIZE);
        
        // initialize song editor
        SongEditor song_editor(
            new Song(4, 8, 8, modctx),
            modctx, window_manager
        );
        song_editor.load_preferences();

        // application quit
        song_editor.ui_actions.set_callback("quit", [&root_window]() {
            glfwSetWindowShouldClose(root_window, 1);
        });

        ui::ui_init(song_editor);

        static const double FRAME_LENGTH = 1.0 / 240.0;

        double next_time = glfwGetTime();
        double prev_time = next_time;

        bool run_app = true;

        // song processing thread
        sys::interval_t* audioaux_interval = sys::set_interval(5, [&]() {
            song_editor.process(device);
        });

        // TODO: run all application logic in another thread (renderer)
        // so that window doesn't freeze when it is being dragged.
        // use glfwWaitEvents(false) on main thread and poll on renderer thread
        glfwShowWindow(root_window);
        while (run_app)
        {
            Song*& song = song_editor.song;
            double now_time = glfwGetTime();

            next_time = glfwGetTime() + FRAME_LENGTH;
            
            glfwPollEvents();

            if (glfwWindowShouldClose(root_window)) {
                glfwSetWindowShouldClose(root_window, 0);
                //glfwFocusWindow(draw_window);
                ui::prompt_unsaved_work([&]() {
                    run_app = false;
                });
            }
#if defined(ENABLE_GTK2) & defined(ENABLE_LV2)
            lv2::gtk_process();
#endif

            // key input
            if (!io.WantTextInput) {
                if (ImGui::IsKeyPressed(ImGuiKey_F1)) {
                    ui::show_demo_window = !ui::show_demo_window;
                }

                // for each user action in the user action list struct
                for (const UserAction& action : song_editor.ui_actions.actions) {
                    if (ImGui::IsKeyPressed(action.key, action.do_repeat)) {
                        // check if all required modifiers are pressed or not pressed
                        if (
                            (ImGui::IsKeyDown(ImGuiMod_Ctrl) == ((action.modifiers & USERMOD_CTRL) != 0) &&
                            ImGui::IsKeyDown(ImGuiMod_Shift) == ((action.modifiers & USERMOD_SHIFT) != 0) &&
                            ImGui::IsKeyDown(ImGuiMod_Alt) == ((action.modifiers & USERMOD_ALT) != 0))
                        ) {
                            if (action.callback)
                                action.callback();
                            else
                                std::cout << "no callback set for " << action.name << "\n";                    
                        }
                    }
                }

                // track editor controls: arrow keys
                if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
                    song_editor.selected_bar++;
                    song_editor.selected_bar %= song->length();
                }

                if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
                    song_editor.selected_bar--;
                    if (song_editor.selected_bar < 0) song_editor.selected_bar = song->length() - 1;
                }

                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
                    song_editor.selected_channel++;
                    song_editor.selected_channel %= song->channels.size();
                }

                if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
                    song_editor.selected_channel--;
                    if (song_editor.selected_channel < 0) song_editor.selected_channel = song->channels.size() - 1;
                }
            }

            int display_w, display_h;
            glfwGetFramebufferSize(draw_window, &display_w, &display_h);
            
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();

            ui::compute_imgui(song_editor);

            // run worker scheduler
            song->work_scheduler.run();

            ImGui::Render();
            window_manager.update();

            glViewport(0, 0, display_w, display_h);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(draw_window);

            prev_time = glfwGetTime();
        }

        sys::clear_interval(audioaux_interval);
        song_editor.save_preferences();
    }

    device.stop();
    AudioDevice::_pa_stop();

#ifdef ENABLE_LV2
    // TODO: interface this function in the PluginManager
    lv2::lv2_fini();
#endif
    return 0;
}
#endif
