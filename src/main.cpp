#include <cassert>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <sstream>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <math.h>
#include <soundio.h>
#include <nfd.h>

#include "audiofile.h"
#include "ui.h"
#include "song.h"
#include "audio.h"
#include "sys.h"

bool IS_BIG_ENDIAN;

static void glfw_error_callback(int error, const char *description)
{
    std::cerr << "GLFW error " << error << ": " << description << "\n";
}

#ifdef USE_WIN32_MAIN
int main();

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow) {
    return main();
}
#endif


int main()
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

    // create window w/ graphics context
    glfwWindowHint(GLFW_VISIBLE, 0); // hide first, show later when everything is ready
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, 1);
    GLFWwindow *window = glfwCreateWindow(1280, 720, "soundbox", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // enable vsync

    float screen_xscale, screen_yscale;
    glfwGetWindowContentScale(window, &screen_xscale, &screen_yscale);

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
        const size_t BUFFER_SIZE = 128;

        AudioDevice device(soundio, -1);
        audiomod::DestinationModule destination(device.sample_rate(), device.num_channels(), BUFFER_SIZE);

        // initialize song
        Song* song = new Song(4, 8, 8, destination);

        // this mutex is locked by the audio thread while audio is being processed
        // and is locked by the main thread when a new song is being loaded
        std::mutex song_mutex;
        
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
                
                song_mutex.lock();
                delete song;
                song = new Song(4, 8, 8, destination);
                ui_init(*song, user_actions);
                song_mutex.unlock();
            };
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
                    song_mutex.lock();

                    std::string error_msg = "unknown error";
                    Song* new_song = Song::from_file(file, destination, &error_msg);
                    file.close();

                    if (new_song != nullptr) {
                        delete song;
                        song = new_song;
                        ui_init(*song, user_actions);

                        last_file_path = out_path;
                        last_file_name = last_file_path.substr(last_file_path.find_last_of("/\\") + 1);
                    } else {
                        status_message = "Error reading file: " + error_msg;
                        status_time = glfwGetTime();
                    }

                    song_mutex.unlock();
                } else {
                    status_message = "Could not open " + std::string(out_path);
                    status_time = glfwGetTime();
                }
            } else if (result != NFD_CANCEL) {
                std::cerr << "Error: " << NFD_GetError() << "\n";
            }
        });

        // application quit
        user_actions.set_callback("quit", [&window]() {
            glfwSetWindowShouldClose(window, 1);
        });

        // actions that don't require window management/io are defined in ui.cpp
        ui_init(*song, user_actions);

        int pattern_input = 0;
        
        // if one of these variables changes, then clear pattern_input
        int last_selected_bar = song->selected_bar;
        int last_selected_ch = song->selected_channel;

        static const double FRAME_LENGTH = 1.0 / 240.0;

        double next_time = glfwGetTime();
        double prev_time = next_time;

        bool run_app = true;

        // exporting
        struct ExportConfigData {
            bool p_open;
            char* file_name;
            int sample_rate;
        } export_config;

        export_config.p_open = false;
        export_config.file_name = nullptr;

        struct ExportProcessData {
            bool is_exporting;
            bool is_done;
            Song* song; // a copy of the current song for the export process
            audiomod::DestinationModule* destination;
            size_t total_frames;
            std::ofstream out_file;
            std::thread* thread;
            audiofile::WavWriter* writer;
        } export_data;

        export_data.is_exporting = false;
        export_data.is_done = false;
        export_data.song = nullptr;
        export_data.total_frames = 0;

        auto export_proc = [&export_data]() {
            audiofile::WavWriter& writer = *export_data.writer;

            export_data.song->bar_position = 0;
            export_data.song->is_playing = true;
            export_data.song->do_loop = false;
            export_data.song->play();

            int step = 0;

            while (export_data.song->is_playing) {
                if (!export_data.is_exporting) break;

                export_data.song->update(1.0 / export_data.destination->sample_rate * export_data.destination->buffer_size);

                float* buf;
                size_t buf_size = export_data.destination->process(&buf);

                writer.write_block(buf, buf_size);

                step++;

                if (step > 1000) {
                    step = 0;
                    sleep(0.001);
                }
            }

            delete export_data.song;
            delete export_data.destination;
            export_data.out_file.close();

            export_data.is_done = true;
        };

        auto begin_export = [&]() {
            // calculate length of song
            const int sample_rate = export_config.sample_rate;

            int beat_len = song->length() * song->beats_per_bar;
            float sec_len = (float)beat_len * (60.0f / song->tempo);
            export_data.total_frames = sec_len * sample_rate;

            // create destination node
            export_data.destination = new audiomod::DestinationModule(sample_rate, 2, 64);

            // create a clone of the song
            std::stringstream song_serialized;
            song->serialize(song_serialized);
            export_data.song = Song::from_file(song_serialized, *export_data.destination, nullptr);

            if (export_data.song == nullptr) {
                status_message = "Error while exporting the song";
                status_time = glfwGetTime();

                delete export_data.destination;
                return;
            }

            // open file
            export_data.out_file.open(export_config.file_name, std::ios::out | std::ios::trunc | std::ios::binary);

            // if could not open file?
            if (!export_data.out_file.is_open()) {
                status_message = "Could not save to " + std::string(export_config.file_name);
                status_time = glfwGetTime();

                delete export_data.song;
                delete export_data.destination;
                return;
            }

            // create writer
            export_data.writer = new audiofile::WavWriter(
                export_data.out_file,
                export_data.total_frames,
                export_data.destination->channel_count,
                export_data.destination->sample_rate
            );

            // create thread to begin export
            export_data.is_exporting = true;
            export_data.is_done = false;
            export_data.thread = new std::thread(export_proc);
        };

        user_actions.set_callback("export", [&]() {
            nfdchar_t* out_path = nullptr;
            nfdresult_t result = NFD_SaveDialog("wav", nullptr, &out_path);

            if (result == NFD_OKAY) {
                export_config.p_open = true;
                export_config.file_name = out_path;
                export_config.sample_rate = 48000;
            } else if (result != NFD_CANCEL) {
                std::cerr << "Error: " << NFD_GetError() << "\n";
                return;
            }
        });

        // create audio processor thread
        std::thread audio_thread([&]() {
            bool last_playing = false;

            while (run_app) {
                song_mutex.lock();
                song->mutex.lock();

                if (song->is_playing != last_playing) {
                    last_playing = song->is_playing;

                    if (song->is_playing) song->play();
                    else song->stop();
                }

                while (device.num_queued_frames() < device.sample_rate() * 0.05) {
                    song->update(1.0 / destination.sample_rate * BUFFER_SIZE);

                    float* buf;
                    size_t buf_size = destination.process(&buf);

                    device.queue(buf, buf_size * sizeof(float));

                    if (!run_app) break;
                }

                song_mutex.unlock();
                song->mutex.unlock();

                sleep(1.0f / 30.0f);
            }
        });

        glfwShowWindow(window);
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

            // show status info as an overlay
            if (now_time < status_time + 2.0) {
                const static float PAD = 10.0f;

                ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoDecoration |
                    ImGuiWindowFlags_NoDocking |
                    ImGuiWindowFlags_AlwaysAutoResize |
                    ImGuiWindowFlags_NoSavedSettings |
                    ImGuiWindowFlags_NoFocusOnAppearing |
                    ImGuiWindowFlags_NoMouseInputs |
                    ImGuiWindowFlags_NoNav;

                const ImGuiViewport* viewport = ImGui::GetMainViewport();
                ImVec2 work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
                ImVec2 work_size = viewport->WorkSize;
                ImVec2 window_pos, window_pos_pivot;
                window_pos.x = work_pos.x + PAD;
                window_pos.y = work_pos.y + work_size.y - PAD;
                window_pos_pivot.x = 0.0f;
                window_pos_pivot.y = 1.0f;
                ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
                ImGui::SetNextWindowViewport(viewport->ID);
                window_flags |= ImGuiWindowFlags_NoMove;
                
                ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
                if (ImGui::Begin("status", nullptr, window_flags)) {
                    ImGui::Text("%s", status_message.c_str());
                    ImGui::End();
                }
            }

            // show new prompt
            if (prompt_unsaved_work) {
                ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                ImGui::OpenPopup("Unsaved work##prompt_new_song");
                prompt_unsaved_work = false;
            }

            if (ImGui::BeginPopupModal("Unsaved work##prompt_new_song", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
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

            // show export prompt
            if (export_config.p_open) {
                if (ImGui::Begin("Export", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                    ImGui::Text("Export path: %s", export_config.file_name);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Choose...")) {
                        user_actions.fire("export");
                    };

                    static int rate_sel = 2;

                    static const char* rate_options[] = {
                        "96 kHz",
                        "88.2 kHz",
                        "48 kHz (recommended)",
                        "44.1 kHz",
                        "22.05 kHz"
                    };

                    static int rate_values[] = {
                        96000,
                        88200,
                        48000,
                        44100,
                        22050
                    };

                    static const char* rate_option = "48 kHz";
                    
                    // sample rate selection
                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("Sample Rate");
                    ImGui::SameLine();
                    if (ImGui::BeginCombo("###export_sample_rate", rate_options[rate_sel])) {
                        for (int i = 0; i < IM_ARRAYSIZE(rate_options); i++) {
                            if (ImGui::Selectable(rate_options[i], rate_sel == i)) {
                                rate_sel = i;
                                export_config.sample_rate = rate_values[i];
                            }

                            if (rate_sel == i) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        
                        ImGui::EndCombo();
                    }

                    if (ImGui::Button("Export")) {
                        if (!export_data.is_exporting) begin_export();
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("Cancel")) {
                        if (export_data.is_exporting) {
                            export_data.is_exporting = false;
                            if (export_data.thread->joinable()) export_data.thread->join();

                            delete export_data.thread;
                            delete export_data.writer;
                        }

                        export_config.p_open = false;
                    }

                    ImVec2 bar_size = ImVec2(ImGui::GetTextLineHeight() * 20.0f, 0.0f);
                    ImGui::SameLine();
                    if (export_data.is_exporting) {
                        ImGui::ProgressBar((float)export_data.writer->written_samples / export_data.writer->total_samples, bar_size);

                        if (export_data.is_done) {
                            if (export_data.thread->joinable()) export_data.thread->join();
                            delete export_data.thread;
                            delete export_data.writer;
                            export_config.p_open = false;

                            status_message = "Successfully exported to " + std::string(export_config.file_name);
                            status_time = now_time;

                            export_data.is_exporting = false;
                        }
                    } else {
                        ImGui::ProgressBar(0.0f, bar_size, "");
                    }

                    ImGui::End();
                }
            }
            
            ImGui::Render();

            glfwSwapBuffers(window);

            glViewport(0, 0, display_w, display_h);
            glClearColor(0.5, 0.7, 0.4, 1.0);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());


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