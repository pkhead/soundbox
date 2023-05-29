#include <cassert>
#include <math.h>
#include <iostream>
#include "audio.h"
#include "modules/modules.h"
#include "imgui.h"
#include "ui.h"

namespace Colors {
    ImU32 channel[channel_num][2] = {
        // { dim, bright }
        { IM_RGB32(187, 17, 17), IM_RGB32(255, 87, 87) },
        { IM_RGB32(187, 128, 17), IM_RGB32(255, 197, 89) },
        { IM_RGB32(136, 187, 17), IM_RGB32(205, 255, 89) },
        { IM_RGB32(25, 187, 17), IM_RGB32(97, 255, 89) },
    };
}

UserAction::UserAction(const std::string& action_name, uint8_t mod, ImGuiKey key) {
    name = action_name;
    set_keybind(mod, key);
}

void UserAction::set_keybind(uint8_t mod, ImGuiKey key) {
    this->modifiers = mod;
    this->key = key;

    // set combo string
    combo.clear();

    if ((mod & USERMOD_SHIFT) != 0) combo += "Shift+";
    if ((mod & USERMOD_CTRL) != 0) combo += "Ctrl+";
    if ((mod & USERMOD_ALT) != 0) combo += "Alt+";
    combo += ImGui::GetKeyName(key);
}

UserActionList::UserActionList() {
    add_action("song_save", USERMOD_CTRL, ImGuiKey_S);
    add_action("song_new", USERMOD_CTRL, ImGuiKey_N);
    add_action("song_save_as", USERMOD_CTRL | USERMOD_SHIFT, ImGuiKey_S);
    add_action("song_open", USERMOD_CTRL, ImGuiKey_O);
    add_action("song_play_pause", 0, ImGuiKey_Space);
    add_action("song_prev_bar", 0, ImGuiKey_LeftBracket);
    add_action("song_next_bar", 0, ImGuiKey_RightBracket);
    add_action("export", 0, ImGuiKey_None);
    add_action("quit", 0, ImGuiKey_None);

    add_action("undo", USERMOD_CTRL, ImGuiKey_Z);
#ifdef _WIN32
    add_action("redo", USERMOD_CTRL, ImGuiKey_Y);
#else
    add_action("redo", USERMOD_CTRL | USERMOD_SHIFT, ImGuiKey_Z);
#endif
    add_action("copy", USERMOD_CTRL, ImGuiKey_C);
    add_action("paste", USERMOD_CTRL, ImGuiKey_V);

    add_action("new_channel", USERMOD_CTRL, ImGuiKey_Enter);

    add_action("mute_channel", USERMOD_CTRL, ImGuiKey_M);
    add_action("solo_channel", USERMOD_ALT, ImGuiKey_M);
}

void UserActionList::add_action(const std::string& action_name, uint8_t mod, ImGuiKey key) {
    actions.push_back(UserAction(action_name, mod, key));
}

void UserActionList::fire(const std::string& action_name) const {
    for (const UserAction& action : actions) {
        if (action.name == action_name) {
            action.callback();
            break;
        }
    }
}

void UserActionList::set_keybind(const std::string& action_name, uint8_t mod, ImGuiKey key) {
    for (UserAction& action : actions) {
        if (action.name == action_name) {
            action.set_keybind(mod, key);
            break;
        }
    }
}

void UserActionList::set_callback(const std::string& action_name, std::function<void()> callback) {
    for (UserAction& action : actions) {
        if (action.name == action_name) {
            action.callback = callback;
            break;
        }
    }
}

const char* UserActionList::combo_str(const std::string& action_name) const {
    for (const UserAction& action : actions) {
        if (action.name == action_name) {
            return action.combo.c_str();
            break;
        }
    }

    return "";
}

















bool show_demo_window;

void ui_init(Song& song, UserActionList& user_actions) {
    // copy+paste
    // TODO use system clipboard
    user_actions.set_callback("copy", [&]() {
        Channel* channel = song.channels[song.selected_channel];
        int pattern_id = channel->sequence[song.selected_bar];
        if (pattern_id > 0) {
            Pattern* pattern = channel->patterns[pattern_id - 1];
            song.note_clipboard = pattern->notes;
        }
    });

    user_actions.set_callback("paste", [&]() {
        if (!song.note_clipboard.empty()) {
            Channel* channel = song.channels[song.selected_channel];
            int pattern_id = channel->sequence[song.selected_bar];
            if (pattern_id > 0) {
                Pattern* pattern = channel->patterns[pattern_id - 1];
                pattern->notes = song.note_clipboard;
            }
        }
    });

    // song play/pause
    user_actions.set_callback("song_play_pause", [&song]() {
        song.is_playing = !song.is_playing;
    });

    // song next bar
    user_actions.set_callback("song_next_bar", [&song]() {
        song.bar_position++;
        song.position += song.beats_per_bar;

        // wrap around
        if (song.bar_position >= song.length()) song.bar_position -= song.length();
        if (song.position >= song.length() * song.beats_per_bar) song.position -= song.length() * song.beats_per_bar;
    });

    // song previous bar
    user_actions.set_callback("song_prev_bar", [&song]() {
        song.bar_position--;
        song.position -= song.beats_per_bar;

        // wrap around
        if (song.bar_position < 0) song.bar_position += song.length();
        if (song.position < 0) song.position += song.length() * song.beats_per_bar;
    });

    // mute selected channel
    user_actions.set_callback("mute_channel", [&song]() {
        Channel* ch = song.channels[song.selected_channel];
        ch->vol_mod.mute = !ch->vol_mod.mute;
    });

    // solo selected channel
    user_actions.set_callback("solo_channel", [&song]() {
        Channel* ch = song.channels[song.selected_channel];
        ch->solo = !ch->solo;
    });

    // create new channel
    user_actions.set_callback("new_channel", [&song]() {
        song.selected_channel++;
        song.insert_channel(song.selected_channel);
    });
}













void compute_imgui(ImGuiIO& io, Song& song, UserActionList& user_actions) {
    ImGuiStyle& style = ImGui::GetStyle();

    static int bus_index = 0;

    static const char* bus_names[] = {
        "0 - master",
        "1 - bus 1",
        "2 - bus 2",
        "3 - drums",
    };

    ImGui::NewFrame();

    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_AutoHideTabBar);
    if (show_demo_window) ImGui::ShowDemoWindow(&show_demo_window);

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New", user_actions.combo_str("song_new")))
                user_actions.fire("song_new");
            if (ImGui::MenuItem("Open", user_actions.combo_str("song_open")))
                user_actions.fire("song_open");
            if (ImGui::MenuItem("Save", user_actions.combo_str("song_save")))
                user_actions.fire("song_save");
            if (ImGui::MenuItem("Save As...", user_actions.combo_str("song_save_as")))
                user_actions.fire("song_save_as");

            ImGui::Separator();

            if (ImGui::MenuItem("Export..."))
                user_actions.fire("export");
            ImGui::MenuItem("Import...");

            ImGui::Separator();

            if (ImGui::MenuItem("Quit", "Alt+F4"))
                user_actions.fire("quit");

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            ImGui::MenuItem("Undo", user_actions.combo_str("undo"));
            ImGui::MenuItem("Redo", user_actions.combo_str("redo"));

            ImGui::Separator();

            ImGui::MenuItem("Select All", user_actions.combo_str("select_all"));
            ImGui::MenuItem("Select Channel", user_actions.combo_str("select_channel"));

            ImGui::Separator();

            ImGui::MenuItem("Copy Pattern", user_actions.combo_str("copy"));
            ImGui::MenuItem("Paste Pattern", user_actions.combo_str("paste"));
            ImGui::MenuItem("Paste Pattern Numbers", user_actions.combo_str("paste_pattern_numbers"));

            ImGui::Separator();

            ImGui::MenuItem("Insert Bar", user_actions.combo_str("insert_bar"));
            ImGui::MenuItem("Delete Bar", user_actions.combo_str("delete_bar"));
            if (ImGui::MenuItem("New Channel", user_actions.combo_str("new_channel")))
                user_actions.fire("new_channel");
            ImGui::MenuItem("Delete Channel", user_actions.combo_str("delete_channel"));
            ImGui::MenuItem("Duplicate Reused Patterns", user_actions.combo_str("duplicate_patterns"));

            ImGui::Separator();

            ImGui::MenuItem("New Pattern", user_actions.combo_str("new_pattern"));
            ImGui::MenuItem("Move Notes Up", user_actions.combo_str("move_notes_up"));
            ImGui::MenuItem("Move Notes Down", user_actions.combo_str("move_notes_down"));
            
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Preferences"))
        {
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help"))
        {
            ImGui::MenuItem("About...");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Dev"))
        {
            if (ImGui::MenuItem("Toggle Dear ImGUI Demo", "F1")) {
                show_demo_window = !show_demo_window;
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    ///////////////////
    // SONG SETTINGS //
    ///////////////////

    if (ImGui::Begin("Song Settings")) {
        // song name input
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Name");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##song_name", song.name, song.name_capcity);

        // play/prev/next
        if (ImGui::Button(song.is_playing ? "Pause##play_pause" : "Play##play_pause", ImVec2(-1.0f, 0.0f)))
            user_actions.fire("song_play_pause");
                
        if (ImGui::Button("Prev", ImVec2(ImGui::GetWindowSize().x / -2.0f, 0.0f))) user_actions.fire("song_prev_bar");
        ImGui::SameLine();
        if (ImGui::Button("Next", ImVec2(-1.0f, 0.0f))) user_actions.fire("song_next_bar");
        
        // tempo
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Tempo (bpm)");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputFloat("bpm##song_tempo", &song.tempo, 0.0f, 0.0f, "%.0f");
        if (song.tempo < 0) song.tempo = 0;

        // TODO: controller/mod channels
        if (ImGui::BeginPopupContextItem()) {
            ImGui::Selectable("Controller 1", true);
            ImGui::Selectable("Controller 2", false);
            ImGui::Selectable("Controller 3", false);
            ImGui::EndPopup();
        }
    } ImGui::End();

    //////////////////////
    // CHANNEL SETTINGS //
    //////////////////////

    Channel* cur_channel = song.channels[song.selected_channel];

    if (ImGui::Begin("Channel Settings")) {
        // channel name
        ImGui::PushItemWidth(-1.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Name");
        ImGui::SameLine();
        ImGui::InputText("##channel_name", cur_channel->name, 16);

        // volume slider
        {
            float volume = cur_channel->vol_mod.volume * 100.0f;
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Volume");
            ImGui::SameLine();
            ImGui::SliderFloat("##channel_volume", &volume, 0, 100, "%.0f");
            cur_channel->vol_mod.volume = volume / 100.0f;
        }

        // panning slider
        {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Panning");
            ImGui::SameLine();
            ImGui::SliderFloat("##channel_panning", &cur_channel->vol_mod.panning, -1, 1, "%.2f");
        }

        // fx mixer bus combobox
        ImGui::AlignTextToFramePadding();
        ImGui::Text("FX Bus");
        ImGui::SameLine();
        if (ImGui::BeginCombo("##channel_bus", bus_names[bus_index]))
        {
            for (int i = 0; i < 4; i++) {
                if (ImGui::Selectable(bus_names[i], i == bus_index)) bus_index = i;

                if (i == bus_index) {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }

        ImGui::PopItemWidth();
        ImGui::NewLine();

        // loaded instrument
        ImGui::Text("Instrument: [No Instrument]");
        if (ImGui::Button("Load...", ImVec2(ImGui::GetWindowSize().x / -2.0f, 0.0f)))
        {
            ImGui::OpenPopup("inst_load");
        }
        ImGui::SameLine();

        if (ImGui::Button("Edit...", ImVec2(-1.0f, 0.0f)))
        {
            song.toggle_module_interface(song.selected_channel, -1);
        }

        // effects
        ImGui::NewLine();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Effects");
        ImGui::SameLine();
        if (ImGui::Button("Add##effect_add")) {
            ImGui::OpenPopup("add_effect");
        }

        if (ImGui::BeginPopup("add_effect")) {
            if (ImGui::Selectable("Gain")) {
                audiomod::ModuleBase* mod = audiomod::create_module("effects.gain");
                cur_channel->effects_rack.insert(mod);
            }

            if (ImGui::Selectable("Analyzer")) {
                audiomod::ModuleBase* mod = audiomod::create_module("effects.analyzer");
                cur_channel->effects_rack.insert(mod);
            }

            ImGui::EndPopup();
        }

        // effects help
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 15.0f);
            ImGui::Text("These apply effects to the instrument before it is sent to the FX bus.");
            ImGui::BulletText("Drag an item to reorder");
            ImGui::BulletText("Double-click to configure");
            ImGui::BulletText("Right-click to remove");
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        { // effects list
            ImVec2 size = ImGui::GetContentRegionAvail();
            size_t i = 0;
            audiomod::EffectsRack& effects_rack = cur_channel->effects_rack;
            
            static int delete_index;
            delete_index = -1;

            for (audiomod::ModuleBase* module : effects_rack.modules) {
                ImGui::PushID(module);

                //bool is_selected = cur_channel->selected_effect == i;
                bool is_selected = false;
                if (ImGui::Selectable(module->name.c_str(), module->show_interface)) {
                    cur_channel->selected_effect = i;
                }

                // drag to reorder
                if (ImGui::IsItemActive() && !ImGui::IsItemHovered()) {
                    int delta = ImGui::GetMouseDragDelta(0).y < 0.f ? -1 : 1;
                    int n_next = i + delta;

                    if (n_next >= 0 && n_next < effects_rack.modules.size()) {
                        // swap n and n_next
                        int min = i > n_next ? n_next : i;
                        audiomod::ModuleBase* m = effects_rack.remove(min);
                        effects_rack.insert(m, min + 1);
                        cur_channel->selected_effect += delta;
                        ImGui::ResetMouseDragDelta();
                    }
                }

                // double click to edit
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    cur_channel->selected_effect = i;
                    song.toggle_module_interface(song.selected_channel, i);
                }

                // right click to delete
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::Selectable("Remove", false)) {
                        // defer deletion until after the loop has finished
                        delete_index = i;
                    }
                    ImGui::EndPopup();
                }

                ImGui::PopID();
                i++;
            }

            // delete the requested module
            if (delete_index >= 0) {
                audiomod::ModuleBase* mod = effects_rack.remove(delete_index);
                if (mod != nullptr) {
                    song.hide_module_interface(mod);
                    delete mod;
                }

                delete_index = -1;
            }
        }
    } ImGui::End();

    if (ImGui::BeginPopup("inst_load"))
    {
        ImGui::Text("test");
        ImGui::EndPopup();
    }

    //////////////////
    // TRACK EDITOR //
    //////////////////
    render_track_editor(io, song);


    ////////////////////
    // PATTERN EDITOR //
    ////////////////////
    render_pattern_editor(io, song);
    

    // render module interfaces
    for (size_t i = 0; i < song.mod_interfaces.size();) {
        auto data = song.mod_interfaces[i];
        
        if (data.module->render_interface(data.channel->name)) i++;
        else {
            song.mod_interfaces.erase(song.mod_interfaces.begin() + i);
        }
    }

    // Info panel //
    if (show_demo_window) {
        ImGui::Begin("Info");
        ImGui::Text("framerate: %.2f", io.Framerate);
        ImGui::End();
    }
}