#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>

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
    glfwSwapInterval(1); // enable vsync

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
    std::string status_message;
    double status_time = -9999.0;

    bool prompt_unsaved_work = false;
    std::function<void()> unsaved_work_callback;

    {
        const size_t BUFFER_SIZE = 1024;

        AudioDevice device(soundio, -1);
        audiomod::DestinationModule destination(device, BUFFER_SIZE);

        // initialize song
        Song* song = new Song(4, 8, 8, destination);
        
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                song->channels[i]->sequence[j] = 1;
            }
        }

        std::function<bool()> save_song;

        auto save_song_as = [&]() -> bool {
            std::string file_name = last_file_name.empty() ? std::string(song->name) + ".box" : last_file_name;
            nfdchar_t* out_path = nullptr;
            nfdresult_t result = NFD_SaveDialog("box", file_name.c_str(), &out_path);

            if (result == NFD_OKAY) {
                last_file_path = out_path;
                last_file_name = last_file_path.substr(last_file_path.find_last_of("/\\") + 1);
                
                save_song();
                
                free(out_path);

                return true;
            }
            else if (result == NFD_CANCEL) {
                return false;
            } else {
                std::cerr << "Error: " << NFD_GetError() << "\n";
                return false;
            }
        };

        save_song = [&]() -> bool {
            if (last_file_path.empty()) {
                return save_song_as();
            }

            std::ofstream file;
            file.open(last_file_path, std::ios::out | std::ios::trunc | std::ios::binary);

            if (file.is_open()) {
                song->serialize(file);
                file.close();

                status_message = "Successfully saved " + last_file_path;
                status_time = glfwGetTime();
                return true;
            } else {
                status_message = "Could not save to " + last_file_path;
                status_time = glfwGetTime();
                return false;
            }
        };

        UserActionList user_actions;

        user_actions.set_callback("song_new", [&]() {
            prompt_unsaved_work = true;
            unsaved_work_callback = [&]() {
                last_file_path.clear();
                last_file_name.clear();
                
                delete song;
                song = new Song(4, 8, 8, destination);
            };
        });

        // song play/pause
        user_actions.set_callback("song_play_pause", [&song]() {
            if (song->is_playing)
                song->stop();
            else
                song->play();
        });

        // song next bar
        user_actions.set_callback("song_next_bar", [&song]() {
            song->bar_position++;
            song->position += song->beats_per_bar;

            // wrap around
            if (song->bar_position >= song->length()) song->bar_position -= song->length();
            if (song->position >= song->length() * song->beats_per_bar) song->position -= song->length() * song->beats_per_bar;
        });

        // song previous bar
        user_actions.set_callback("song_prev_bar", [&song]() {
            song->bar_position--;
            song->position -= song->beats_per_bar;

            // wrap around
            if (song->bar_position < 0) song->bar_position += song->length();
            if (song->position < 0) song->position += song->length() * song->beats_per_bar;
        });

        // song save as
        user_actions.set_callback("song_save_as", [&save_song_as]() {
            save_song_as();
        });

        // song save
        user_actions.set_callback("song_save", [&save_song]() {
            save_song();
        });

        // song open
        user_actions.set_callback("song_open", [&]() {
            nfdchar_t* out_path;
            nfdresult_t result = NFD_OpenDialog("box", nullptr, &out_path);

            if (result == NFD_OKAY) {
                std::ifstream file;
                file.open(out_path, std::ios::in | std::ios::binary);

                if (file.is_open()) {
                    Song* new_song = Song::from_file(file, destination);
                    file.close();

                    if (new_song != nullptr) {
                        delete song;
                        song = new_song;
                    } else {
                        status_message = "Error reading file";
                        status_time = glfwGetTime();
                    }
                } else {
                    status_message = "Could not open " + std::string(out_path);
                    status_time = glfwGetTime();
                }
            } else if (result != NFD_CANCEL) {
                std::cerr << "Error: " << NFD_GetError() << "\n";
            }
        });

        // copy+paste
        // TODO use system clipboard
        user_actions.set_callback("copy", [&]() {
            Channel* channel = song->channels[song->selected_channel];
            int pattern_id = channel->sequence[song->selected_bar];
            if (pattern_id > 0) {
                Pattern* pattern = channel->patterns[pattern_id - 1];
                song->note_clipboard = pattern->notes;
            }
        });

        user_actions.set_callback("paste", [&]() {
            if (!song->note_clipboard.empty()) {
                Channel* channel = song->channels[song->selected_channel];
                int pattern_id = channel->sequence[song->selected_bar];
                if (pattern_id > 0) {
                    Pattern* pattern = channel->patterns[pattern_id - 1];
                    pattern->notes = song->note_clipboard;
                }
            }
        });

        // application quit
        user_actions.set_callback("quit", [&window]() {
            glfwSetWindowShouldClose(window, 1);
        });

        int pattern_input = 0;
        
        // if one of these variables changes, then clear pattern_input
        int last_selected_bar = song->selected_bar;
        int last_selected_ch = song->selected_channel;

        static const double FRAME_LENGTH = 1.0 / 240.0;

        double next_time = glfwGetTime();
        double prev_time = next_time;

        bool run_app = true;

        // create audio processor thread
        std::thread audio_thread([&]() {
            while (run_app) {
                while (device.num_queued_frames() < device.sample_rate() * 0.05) {
                    song->update(1.0 / device.sample_rate() * BUFFER_SIZE);

                    float* buf;
                    size_t buf_size = destination.process(&buf);

                    device.queue(buf, buf_size * sizeof(float));
                }

                sleep(1.0f / 30.0f);
            }
        });

        while (run_app)
        {
            if (glfwWindowShouldClose(window)) {
                glfwSetWindowShouldClose(window, 0);
                prompt_unsaved_work = true;
                unsaved_work_callback = [&]() {
                    run_app = false;
                };
            }

            double now_time = glfwGetTime();

            next_time = glfwGetTime() + FRAME_LENGTH;
            
            glfwPollEvents();

            //song->update(now_time - prev_time);

            // if selected pattern changed
            if (last_selected_bar != song->selected_bar || last_selected_ch != song->selected_channel) {
                last_selected_bar = song->selected_bar;
                last_selected_ch = song->selected_channel;
                pattern_input = 0;
            }

            // key input
            if (!io.WantTextInput) {
                if (ImGui::IsKeyPressed(ImGuiKey_F1)) {
                    show_demo_window = !show_demo_window;
                }

                // for each user action in the user action list struct
                for (const UserAction& action : user_actions.actions) {
                    if (ImGui::IsKeyPressed(action.key, false)) {
                        std::cout << action.key << "\n";
                        
                        // check if all required modifiers are pressed or not pressed
                        if (
                            action.modifiers == 0 ||
                            (ImGui::IsKeyDown(ImGuiMod_Ctrl) == ((action.modifiers & USERMOD_CTRL) != 0) &&
                            ImGui::IsKeyDown(ImGuiMod_Shift) == ((action.modifiers & USERMOD_SHIFT) != 0) &&
                            ImGui::IsKeyDown(ImGuiMod_Alt) == ((action.modifiers & USERMOD_ALT) != 0))
                        ) {
                            std::cout << action.name << "\n";
                            action.callback();
                        }
                    }
                }

                // track editor controls: arrow keys
                if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
                    song->selected_bar++;
                    song->selected_bar %= song->length();
                }

                if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
                    song->selected_bar--;
                    if (song->selected_bar < 0) song->selected_bar = song->length() - 1;
                }

                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
                    song->selected_channel++;
                    song->selected_channel %= song->channels.size();
                }

                if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
                    song->selected_channel--;
                    if (song->selected_channel < 0) song->selected_channel = song->channels.size() - 1;
                }

                // track editor pattern entering: number keys
                for (int k = 0; k < 10; k++) {
                    if (ImGui::IsKeyPressed((ImGuiKey)((int)ImGuiKey_0 + k))) {
                        pattern_input = (pattern_input * 10) + k;
                        if (pattern_input > song->max_patterns()) pattern_input = k;

                        if (pattern_input <= song->max_patterns())
                            song->channels[song->selected_channel]->sequence[song->selected_bar] = pattern_input;
                    }
                }
            }

            int display_w, display_h;
            glfwGetFramebufferSize(window, &display_w, &display_h);
            
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();

            compute_imgui(io, *song, user_actions);

            // show status info as a tooltip
            if (now_time < status_time + 2.0) {
                ImGui::SetTooltip("%s", status_message.c_str());
            }

            // show new prompt
            if (prompt_unsaved_work) {
                ImGui::OpenPopup("Unsaved work##prompt_new_song");
                prompt_unsaved_work = false;
            }

            if (ImGui::BeginPopupModal("Unsaved work##prompt_new_song", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
                ImGui::Text("Do you want to save your work before continuing?");
                ImGui::NewLine();
                
                if (ImGui::Button("Yes##popup_modal_yes")) {
                    ImGui::CloseCurrentPopup();
                    
                    // if song was able to successfully be saved, then continue
                    if (save_song()) unsaved_work_callback();
                }

                ImGui::SameLine();
                if (ImGui::Button("No##popup_modal_no")) {
                    ImGui::CloseCurrentPopup();
                    unsaved_work_callback();
                }

                ImGui::SameLine();
                if (ImGui::Button("Cancel##popup_modal_cancel")) {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
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
            
            //sleep(next_time - cur_time);
        }

        audio_thread.join();

        delete song;
    }

    soundio_destroy(soundio);

    return 0;
}