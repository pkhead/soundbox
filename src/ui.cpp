#include <cassert>
#include <cstdio>
#include <math.h>
#include <iostream>
#include <string>
#include "audio.h"
#include "modules/modules.h"
#include "imgui.h"
#include "song.h"
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

UserAction::UserAction(const std::string& action_name, uint8_t mod, ImGuiKey key, bool repeat) {
    name = action_name;
    do_repeat = repeat;
    set_keybind(mod, key);
}

void UserAction::set_keybind(uint8_t mod, ImGuiKey key) {
    this->modifiers = mod;
    this->key = key;

    // set combo string
    combo.clear();

    if (key != ImGuiKey_None)
    {
        if ((mod & USERMOD_SHIFT) != 0) combo += "Shift+";
        if ((mod & USERMOD_CTRL) != 0)  combo += "Ctrl+";
        if ((mod & USERMOD_ALT) != 0)   combo += "Alt+";
        combo += ImGui::GetKeyName(key);
    }
}

UserActionList::UserActionList() {
    add_action("song_save", USERMOD_CTRL, ImGuiKey_S);
    add_action("song_new", USERMOD_CTRL, ImGuiKey_N);
    add_action("song_save_as", USERMOD_CTRL | USERMOD_SHIFT, ImGuiKey_S);
    add_action("song_open", USERMOD_CTRL, ImGuiKey_O);
    add_action("song_play_pause", 0, ImGuiKey_Space);
    add_action("song_prev_bar", 0, ImGuiKey_LeftBracket, true);
    add_action("song_next_bar", 0, ImGuiKey_RightBracket, true);
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

    add_action("new_channel", USERMOD_CTRL, ImGuiKey_Enter, true);
    add_action("remove_channel", USERMOD_CTRL, ImGuiKey_Backspace, true);
    add_action("insert_bar", 0, ImGuiKey_Enter, true);
    add_action("insert_bar_before", USERMOD_SHIFT, ImGuiKey_Enter, true);
    add_action("remove_bar", 0, ImGuiKey_Backspace);

    add_action("mute_channel", USERMOD_CTRL, ImGuiKey_M);
    add_action("solo_channel", USERMOD_SHIFT, ImGuiKey_M);

    add_action("load_tuning", 0, ImGuiKey_None);
}

void UserActionList::add_action(const std::string& action_name, uint8_t mod, ImGuiKey key, bool repeat) {
    actions.push_back(UserAction(action_name, mod, key, repeat));
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

void push_btn_disabled(ImGuiStyle& style, bool is_disabled)
{
    ImVec4 btn_colors[3];
    btn_colors[0] = style.Colors[ImGuiCol_Button];
    btn_colors[1] = style.Colors[ImGuiCol_ButtonHovered];
    btn_colors[2] = style.Colors[ImGuiCol_ButtonActive];

    if (is_disabled)
    {
        for (int i = 0; i < 3; i++) {
            btn_colors[i].w *= 0.5f;
        }
    }

    ImGui::PushStyleColor(ImGuiCol_Button, btn_colors[0]);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btn_colors[1]);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, btn_colors[2]);
}

void pop_btn_disabled()
{
    ImGui::PopStyleColor(3);
}















bool show_demo_window = false;
bool show_about_window = false;

void ui_init(Song& song, UserActionList& user_actions) {
    show_about_window = false;

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

    // delete selected channel
    user_actions.set_callback("remove_channel", [&song]() {
        if (song.channels.size() > 1)
        {
            song.remove_channel(song.selected_channel);
            if (song.selected_channel > 0) song.selected_channel--;
        }
    });

    // insert bar
    user_actions.set_callback("insert_bar", [&song]() {
        song.selected_bar++;
        song.insert_bar(song.selected_bar);
    });

    // insert bar before
    user_actions.set_callback("insert_bar_before", [&song]() {
        song.insert_bar(song.selected_bar);
    });

    // delete bar
    user_actions.set_callback("remove_bar", [&song]() {
        if (song.length() > 1)
        {
            song.remove_bar(song.selected_bar);
            if (song.selected_bar > 0) song.selected_bar--;
        }
    });
}











EffectsInterfaceAction effect_rack_ui(std::mutex &mutex, audiomod::EffectsRack* effects_rack, const char* parent_name, int* selected_index)
{
    EffectsInterfaceAction action = EffectsInterfaceAction::Nothing;

    // effects
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Effects");
    ImGui::SameLine();
    if (ImGui::Button("Add##effect_add")) {
        ImGui::OpenPopup("add_effect");
    }

    if (ImGui::BeginPopup("add_effect")) {
        if (ImGui::Selectable("Gain")) {
            mutex.lock();
            audiomod::ModuleBase* mod = audiomod::create_module("effects.gain");
            mod->parent_name = parent_name;
            effects_rack->insert(mod);
            mutex.unlock();
        }

        if (ImGui::Selectable("Analyzer")) {
            mutex.lock();
            audiomod::ModuleBase* mod = audiomod::create_module("effects.analyzer");
            mod->parent_name = parent_name;
            effects_rack->insert(mod);
            mutex.unlock();
        }

        ImGui::EndPopup();
    }

    // effects help
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 15.0f);
        ImGui::BulletText("Drag an item to reorder");
        ImGui::BulletText("Double-click to configure");
        ImGui::BulletText("Right-click to remove");
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    { // effects list
        ImVec2 size = ImGui::GetContentRegionAvail();
        size_t i = 0;
        
        static int delete_index;
        delete_index = -1;

        for (audiomod::ModuleBase* module : effects_rack->modules) {
            ImGui::PushID(module);

            //bool is_selected = cur_channel->selected_effect == i;
            bool is_selected = false;
            ImGui::Selectable(module->name.c_str(), module->show_interface);

            // drag to reorder
            if (ImGui::IsItemActive() && !ImGui::IsItemHovered()) {
                int delta = ImGui::GetMouseDragDelta(0).y < 0.f ? -1 : 1;
                int n_next = i + delta;

                if (n_next >= 0 && n_next < effects_rack->modules.size()) {
                    // swap n and n_next
                    int min = i > n_next ? n_next : i;
                    mutex.lock();
                    audiomod::ModuleBase* m = effects_rack->remove(min);
                    effects_rack->insert(m, min + 1);
                    mutex.unlock();
                    ImGui::ResetMouseDragDelta();
                }
            }

            // double click to edit
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                *selected_index = i;
                action = EffectsInterfaceAction::Edit;
            }

            // right click to delete
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::Selectable("Remove", false)) {
                    // defer deletion until after the loop has finished
                    *selected_index = i;
                    action = EffectsInterfaceAction::Delete;
                }
                ImGui::EndPopup();
            }

            ImGui::PopID();
            i++;
        }
    }

    return action;
}

// menu item helper
#define MENU_ITEM(label, action_name) \
    if (ImGui::MenuItem(label, user_actions.combo_str(action_name))) \
        deferred_actions.push_back(action_name)

void compute_imgui(ImGuiIO& io, Song& song, UserActionList& user_actions) {
    ImGuiStyle& style = ImGui::GetStyle();
    static char char_buf[64];

    ImGui::NewFrame();

    std::vector<const char*> deferred_actions;

    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_AutoHideTabBar);
    if (show_demo_window) ImGui::ShowDemoWindow(&show_demo_window);

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            MENU_ITEM("New", "song_new");
            MENU_ITEM("Open", "song_open");
            MENU_ITEM("Save", "song_save");
            MENU_ITEM("Save As...", "song_save_as");
            
            ImGui::Separator();

            MENU_ITEM("Export...", "export");
            MENU_ITEM("Import...", "import");

            ImGui::Separator();

            if (ImGui::MenuItem("Quit", "Alt+F4"))
                user_actions.fire("quit");
            
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            MENU_ITEM("Undo", "undo");
            MENU_ITEM("Redo", "redo");

            ImGui::Separator();

            MENU_ITEM("Select All", "select_all");
            MENU_ITEM("Select Channel", "select_channel");

            ImGui::Separator();

            MENU_ITEM("Copy Pattern", "copy");
            MENU_ITEM("Paste Pattern", "paste");
            MENU_ITEM("Paste Pattern Numbers", "paste_pattern_numbers");

            ImGui::Separator();

            MENU_ITEM("Insert Bar", "insert_bar");
            MENU_ITEM("Insert Bar Before", "insert_bar_before");
            MENU_ITEM("Delete Bar", "remove_bar");
            MENU_ITEM("New Channel", "new_channel");
            MENU_ITEM("Delete Channel", "remove_channel");
            MENU_ITEM("Duplicate Reused Patterns", "duplicate_patterns");

            ImGui::Separator();

            MENU_ITEM("New Pattern", "new_pattern");
            MENU_ITEM("Move Notes Up", "move_notes_up");
            MENU_ITEM("Move Notes Down", "move_notes_down");

            ImGui::Separator();

            if (ImGui::MenuItem("Tuning..."))
                song.show_tuning_window = !song.show_tuning_window;
            
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Preferences"))
        {
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("About..."))
                show_about_window = !show_about_window;
            
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
        ImGui::Text("Tempo");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::DragFloat("###song_tempo", &song.tempo, 0.1f, 0.0f, 5000.0f, "%.3f");
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

        
        {
            // fx mixer bus combobox
            ImGui::AlignTextToFramePadding();
            ImGui::Text("FX Output");
            ImGui::SameLine();

            // write preview value
            audiomod::FXBus* cur_fx_bus = song.fx_mixer[cur_channel->fx_target_idx];
            snprintf(char_buf, 64, "%i - %s", cur_channel->fx_target_idx, cur_fx_bus->name);

            if (ImGui::BeginCombo("##channel_fx_target", char_buf))
            {
                // list potential targets
                for (size_t target_i = 0; target_i < song.fx_mixer.size(); target_i++)
                {
                    audiomod::FXBus* target_bus = song.fx_mixer[target_i];

                    // write target bus name
                    snprintf(char_buf, 64, "%lu - %s", target_i, target_bus->name);

                    bool is_selected = target_i == cur_channel->fx_target_idx;
                    if (ImGui::Selectable(char_buf, is_selected))
                        cur_channel->set_fx_target(target_i);

                    if (is_selected) ImGui::SetItemDefaultFocus();
                }
                
                ImGui::EndCombo();
            }
        }

        ImGui::PopItemWidth();
        ImGui::NewLine();

        // loaded instrument
        ImGui::Text("Instrument: %s", cur_channel->synth_mod->name.c_str());
        if (ImGui::Button("Load...", ImVec2(ImGui::GetWindowSize().x / -2.0f, 0.0f)))
        {
            ImGui::OpenPopup("inst_load");
        }
        ImGui::SameLine();

        if (ImGui::Button("Edit...", ImVec2(-1.0f, 0.0f)))
        {
            song.toggle_module_interface(cur_channel->synth_mod);
        }

        int target_index;
        switch (effect_rack_ui(song.mutex, &cur_channel->effects_rack, cur_channel->name, &target_index))
        {
            case EffectsInterfaceAction::Edit:
                song.toggle_module_interface(cur_channel->effects_rack.modules[target_index]);
                break;

            case EffectsInterfaceAction::Delete: {
                // delete the selected module
                song.mutex.lock();

                audiomod::ModuleBase* mod = cur_channel->effects_rack.remove(target_index);
                if (mod != nullptr) {
                    song.hide_module_interface(mod);
                    delete mod;
                }

                song.mutex.unlock();
                break;
            }

            case EffectsInterfaceAction::Nothing: break;
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

    ////////////////////
    //    FX MIXER    //
    ////////////////////
    if (ImGui::Begin("FX Mixer"))
    {
        int i = 0;

        float frame_width = ImGui::CalcTextSize("MMMMMMMMMMMMMMMM").x + style.FramePadding.x;
        
        float mute_btn_size = ImGui::CalcTextSize("M").x + style.FramePadding.x * 2.0f;
        float solo_btn_size = ImGui::CalcTextSize("S").x + style.FramePadding.x * 2.0f;

        for (audiomod::FXBus* bus : song.fx_mixer)
        {
            ImGui::PushID(i);

            float width = ImGui::GetContentRegionAvail().x;
            
            std::string label = std::to_string(i) + " - " + bus->name;
            if (ImGui::Selectable(
                label.c_str(),
                bus->interface_open,
                0,
                Vec2(width - mute_btn_size - solo_btn_size - style.ItemSpacing.x * 2.0f, 0.0f)
            ))
            // if selectable is clicked,
                bus->interface_open = !bus->interface_open;

            // mute button
            push_btn_disabled(style, !bus->controller.mute);
            ImGui::SameLine();
            if (ImGui::SmallButton("M"))
            {
                bus->controller.mute = !bus->controller.mute;
            }
            pop_btn_disabled();

            // solo button
            push_btn_disabled(style, !bus->solo);
            ImGui::SameLine();
            if (ImGui::SmallButton("S"))
            {
                bus->solo = !bus->solo;
            }
            pop_btn_disabled();

            // left channel
            ImGui::ProgressBar(bus->controller.analysis_volume[0], Vec2(-1.0f, 1.0f), "");
            // right channel
            ImGui::ProgressBar(bus->controller.analysis_volume[1], Vec2(-1.0f, 1.0f), "");

            ImGui::PopID();
            
            i++;
        }

        ImGui::Separator();
        if (ImGui::Button("Add", ImVec2(-1.0f, 0.0f)))
        {
            song.mutex.lock();

            audiomod::FXBus* bus = new audiomod::FXBus;
            song.fx_mixer[0]->connect_input(&bus->controller);
            song.fx_mixer.push_back(bus);

            song.mutex.unlock();
        }
    } ImGui::End();
    

    // render module interfaces
    for (size_t i = 0; i < song.mod_interfaces.size();)
    {
        auto mod = song.mod_interfaces[i];
        
        if (mod->render_interface()) i++;
        else {
            song.mod_interfaces.erase(song.mod_interfaces.begin() + i);
        }
    }

    // render fx interfaces
    int i = 0;

    for (audiomod::FXBus* fx_bus : song.fx_mixer)
    {
        if (!fx_bus->interface_open)
        {
            i++;
            continue;
        }
        
        // write window id
        snprintf(char_buf, 64, "FX: %i - %s###%p", i, fx_bus->name, fx_bus);
        
        if (ImGui::Begin(
            char_buf, // window id
            &fx_bus->interface_open,
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking
        )) {
            // set next item width to make sure window title is not truncated
            ImGui::SetNextItemWidth(ImGui::CalcTextSize("FX: 0 MM - MMMMMMMMMMMMMMMMMM").x);
            ImGui::InputText("##name", fx_bus->name, fx_bus->name_capacity);

            // show delete button
            if (i > 0)
            {
                ImGui::SameLine();
                ImGui::Button("Delete");
            }

            // show volume analysis and TODO: gain slider
            // left channel
            float bar_height = ImGui::GetTextLineHeight() * 0.25f;
            ImGui::ProgressBar(fx_bus->controller.analysis_volume[0], Vec2(-1.0f, bar_height), "");
            // right channel
            ImGui::ProgressBar(fx_bus->controller.analysis_volume[1], Vec2(-1.0f, bar_height), "");

            // show output bus combobox
            if (i > 0)
            {
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Output Bus");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(-1.0f);
                // write preview value
                snprintf(char_buf, 64, "%i - %s", fx_bus->target_bus, song.fx_mixer[fx_bus->target_bus]->name);

                if (ImGui::BeginCombo("##fx_target", char_buf))
                {
                    // list potential targets
                    for (size_t target_i = 0; target_i < song.fx_mixer.size(); target_i++)
                    {
                        audiomod::FXBus* target_bus = song.fx_mixer[target_i];

                        // skip if looking at myself or an input of myself
                        if (target_bus == fx_bus || song.fx_mixer[target_bus->target_bus] == fx_bus) continue;

                        // write target bus name
                        snprintf(char_buf, 64, "%lu - %s", target_i, target_bus->name);

                        bool is_selected = target_i == fx_bus->target_bus;
                        if (ImGui::Selectable(char_buf, is_selected))
                        {
                            // remove old connection
                            song.fx_mixer[fx_bus->target_bus]->disconnect_input(&fx_bus->controller);

                            // create new connection
                            fx_bus->target_bus = target_i;
                            target_bus->connect_input(&fx_bus->controller);
                        }

                        if (is_selected) ImGui::SetItemDefaultFocus();
                    }
                    
                    ImGui::EndCombo();
                }
            }

            int target_index;
            switch (effect_rack_ui(song.mutex, &fx_bus->rack, fx_bus->name, &target_index))
            {
                case EffectsInterfaceAction::Edit:
                    song.toggle_module_interface(fx_bus->rack.modules[target_index]);
                    break;

                case EffectsInterfaceAction::Delete: {
                    // delete the selected module
                    song.mutex.lock();

                    audiomod::ModuleBase* mod = fx_bus->rack.remove(target_index);
                    if (mod != nullptr) {
                        song.hide_module_interface(mod);
                        delete mod;
                    }

                    song.mutex.unlock();
                    break;
                }

                case EffectsInterfaceAction::Nothing: break;
            }
        }

        ImGui::End();

        i++;
    }

    // tunings window
    if (song.show_tuning_window) {
        if (ImGui::Begin("Tunings", &song.show_tuning_window))
        {
            // TODO: remove tunings
            if (ImGui::Button("Load"))
                user_actions.fire("load_tuning");
            
            if (ImGui::BeginChild("list", ImVec2(ImGui::GetContentRegionAvail().x * 0.4f, -1.0f)))
            {
                int i = 0;

                for (Tuning* tuning : song.tunings)
                {
                    ImGui::PushID(i);

                    if (ImGui::Selectable(tuning->name.c_str(), i == song.selected_tuning))
                    {
                        song.selected_tuning = i;
                    }

                    if (song.selected_tuning == i) ImGui::SetItemDefaultFocus();

                    ImGui::PopID();
                    i++;
                }
            }
            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginGroup();

            Tuning* selected_tuning = song.tunings[song.selected_tuning];

            ImGui::Text("Description");
            ImGui::Separator();

            if (selected_tuning->desc.empty())
                ImGui::TextDisabled("(no description)");
            else
                ImGui::TextWrapped("%s", selected_tuning->desc.c_str());

            ImGui::EndGroup();
        } ImGui::End();
    }

    // Info panel //
    if (show_demo_window) {
        ImGui::Begin("Info");
        ImGui::Text("framerate: %.2f", io.Framerate);
        ImGui::End();
    }

    // about window
    if (show_about_window && ImGui::Begin(
        "About",
        &show_about_window,
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize))
    {
        float window_width = ImGui::GetWindowWidth();
        ImGui::PushTextWrapPos(500.0f);
        
        // draw logo
        if (logo_texture)
        {
            ImGui::SetCursorPosX((window_width - (float)logo_width) / 2.0f);
            ImGui::Image((void*)(intptr_t)logo_texture, ImVec2(logo_width, logo_height));
        }
        
        // app version / file format version
        const char* version = "version x.x.x / 0.0.3";
        ImGui::SetCursorPosX((window_width - ImGui::CalcTextSize(version).x) / 2.0f);
        ImGui::Text("version x.x.x / 0.0.3");

        ImGui::NewLine();
        ImGui::TextWrapped("Thanks to John Nesky's BeepBox");
        ImGui::NewLine();
        ImGui::TextWrapped("Open-source libraries:");

        ImGui::Bullet();
        ImGui::TextWrapped("libsoundio: https://libsound.io/");
        ImGui::Bullet();
        ImGui::TextWrapped("Dear ImGui: https://www.dearimgui.com/");
        ImGui::Bullet();
        ImGui::TextWrapped("glfw: https://www.glfw.org/");
        ImGui::Bullet();
        ImGui::TextWrapped("AnaMark tuning library: https://github.com/zardini123/AnaMark-Tuning-Library");
        ImGui::Bullet();
        ImGui::TextWrapped("nativefiledialog: https://github.com/mlabbe/nativefiledialog");
        ImGui::Bullet();
        ImGui::TextWrapped("stb_image: https://github.com/nothings/stb");
        ImGui::End();
    }

    // run all scheduled actions
    // (run them last so that the song pointer doesn't change mid-function
    // and cause memory errors)
    for (const char* action_name : deferred_actions)
    {
        user_actions.fire(action_name);
    }
}