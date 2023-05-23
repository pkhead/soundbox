#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "../imgui/imgui.h"
#include "../imgui/backends/imgui_impl_glfw.h"
#include "../imgui/backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <math.h>
#include <soundio.h>
#include <nfd.h>

#include "ui.h"
#include "song.h"
#include "audio.h"
#include "os.h"

static void glfw_error_callback(int error, const char *description)
{
    std::cerr << "GLFW error " << error << ": " << description << "\n";
}

int main()
{
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

    // create window w/ graphics context
    GLFWwindow *window = glfwCreateWindow(1280, 720, "soundbox", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0); // disable vsync

    // setup dear imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImGui::StyleColorsClassic();

    // setup soundio
    int err;

    SoundIo* soundio = soundio_create();
    if (!soundio) {
        std::cerr << "out of memory!\n";
        return 1;
    }

    if ((err = soundio_connect(soundio))) {
        std::cerr << "error connecting: " << soundio_strerror(err) << "\n";
        return 1;
    }

    soundio_flush_events(soundio);

    show_demo_window = false;

    std::string last_file_path;
    std::string last_file_name;

    {
        const size_t BUFFER_SIZE = 1024;

        AudioDevice device(soundio, -1);
        audiomod::DestinationModule destination(device, BUFFER_SIZE);

        // initialize song
        Song song(4, 8, 8, destination);

        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                song.channels[i]->sequence[j] = 1;
            }
        }

        UserActionList user_actions;

        // song play/pause
        user_actions.song_play_pause = [&song]() {
            if (song.is_playing)
                song.stop();
            else
                song.play();
        };

        // song next bar
        user_actions.song_next_bar = [&song]() {
            song.bar_position++;
            song.position += song.beats_per_bar;

            // wrap around
            if (song.bar_position >= song.length()) song.bar_position -= song.length();
            if (song.position >= song.length() * song.beats_per_bar) song.position -= song.length() * song.beats_per_bar;
        };

        // song previous bar
        user_actions.song_prev_bar = [&song]() {
            song.bar_position--;
            song.position -= song.beats_per_bar;

            // wrap around
            if (song.bar_position < 0) song.bar_position += song.length();
            if (song.position < 0) song.position += song.length() * song.beats_per_bar;
        };

        // song save
        user_actions.song_save_as = [&]() {
            std::string file_name = last_file_name.empty() ? std::string(song.name) + ".box" : last_file_name;
            nfdchar_t* out_path = nullptr;
            nfdresult_t result = NFD_SaveDialog("box", file_name.c_str(), &out_path);

            if (result == NFD_OKAY) {
                last_file_path = out_path;
                last_file_name = last_file_path.substr(last_file_path.find_last_of("/\\") + 1);

                user_actions.song_save();
                
                free(out_path);
            }
            else if (result == NFD_CANCEL) {
                std::cout << "User pressed cancel.\n";
            } else {
                std::cerr << "Error: " << NFD_GetError() << "\n";
            }
        };

        std::string status_message;
        double status_time = -9999.0;

        user_actions.song_save = [&]() {
            if (last_file_path.empty()) {
                user_actions.song_save_as();
                return;
            }

            std::ofstream file;
            file.open(last_file_path, std::ios::out | std::ios::trunc | std::ios::binary);

            if (file.is_open()) {
                song.serialize(file);
                file.close();

                status_message = "Successfully saved " + last_file_path;
                status_time = glfwGetTime();
            } else {
                status_message = "Could not open " + last_file_path;
                status_time = glfwGetTime();
            }
        };

        // application quit
        user_actions.quit = [&window]() {
            glfwSetWindowShouldClose(window, 1);
        };

        int pattern_input = 0;
        
        // if one of these variables changes, then clear pattern_input
        int last_selected_bar = song.selected_bar;
        int last_selected_ch = song.selected_channel;

        static const double FRAME_LENGTH = 1.0 / 120.0;

        double next_time = glfwGetTime();
        double prev_time = next_time;

        while (!glfwWindowShouldClose(window))
        {
            double now_time = glfwGetTime();

            next_time = glfwGetTime() + FRAME_LENGTH;
            
            glfwPollEvents();

            int num_buffers = 0;

            while (device.num_queued_frames() < device.sample_rate() * 0.05) {
                float* buf;
                size_t buf_size = destination.process(&buf);

                device.queue(buf, buf_size * sizeof(float));
                num_buffers++;
            }

            song.update(now_time - prev_time);

            // if selected pattern changed
            if (last_selected_bar != song.selected_bar || last_selected_ch != song.selected_channel) {
                last_selected_bar = song.selected_bar;
                last_selected_ch = song.selected_channel;
                pattern_input = 0;
            }

            // key input
            if (!io.WantTextInput) {
                if (ImGui::IsKeyPressed(ImGuiKey_F1)) {
                    show_demo_window = !show_demo_window;
                }

                // save: Ctrl+S
                if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_S, false)) user_actions.song_save();

                // save as: Ctrl+Shift+S
                if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyDown(ImGuiMod_Shift) && ImGui::IsKeyPressed(ImGuiKey_S, false)) user_actions.song_save_as();

                // track editor controls: arrow keys
                if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
                    song.selected_bar++;
                    song.selected_bar %= song.length();
                }

                if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
                    song.selected_bar--;
                    if (song.selected_bar < 0) song.selected_bar = song.length() - 1;
                }

                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
                    song.selected_channel++;
                    song.selected_channel %= song.channels.size();
                }

                if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
                    song.selected_channel--;
                    if (song.selected_channel < 0) song.selected_channel = song.channels.size() - 1;
                }

                // track editor pattern entering: number keys
                for (int k = 0; k < 10; k++) {
                    if (ImGui::IsKeyPressed((ImGuiKey)((int)ImGuiKey_0 + k))) {
                        pattern_input = (pattern_input * 10) + k;
                        if (pattern_input > song.max_patterns()) pattern_input = k;

                        if (pattern_input <= song.max_patterns())
                            song.channels[song.selected_channel]->sequence[song.selected_bar] = pattern_input;
                    }
                }

                // play/pause: space
                if (ImGui::IsKeyPressed(ImGuiKey_Space)) user_actions.song_play_pause();

                // song prev/next bar: left/right bracket keys
                if (ImGui::IsKeyPressed(ImGuiKey_RightBracket)) user_actions.song_next_bar();
                if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket)) user_actions.song_prev_bar();
            }

            int display_w, display_h;
            glfwGetFramebufferSize(window, &display_w, &display_h);
            
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();

            compute_imgui(io, song, user_actions);

            if (now_time < status_time + 2.0) {
                ImGui::SetTooltip("%s", status_message.c_str());
            }
            
            ImGui::Render();

            glViewport(0, 0, display_w, display_h);
            glClearColor(0.5, 0.7, 0.4, 1.0);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(window);

            prev_time = glfwGetTime();

            double cur_time = glfwGetTime();

            if (next_time <= cur_time) {
                next_time = cur_time;
            }
            
            sleep(next_time - cur_time);

        }
    }
    soundio_destroy(soundio);

    return 0;
}