#include <cassert>
#include <cstdio>
#include <math.h>
#include <iostream>
#include <string>
#include <imgui.h>
#include "../audio.h"
#include "change_history.h"
#include "../imgui_stdlib.h"
#include "../modules/modules.h"
#include "../song.h"
#include "editor.h"
#include "theme.h"
#include "ui.h"

constexpr char APP_VERSION[] = "0.1.0";
constexpr char FILE_VERSION[] = "0001";
char VERSION_STR[64];

ImU32 hex_color(const char* str)
{
    int r, g, b;
    sscanf(str, "%02x%02x%02x", &r, &g, &b);
    return IM_RGB32(r, g, b);    
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
bool show_shortcuts_window = false;

std::string status_message;
double status_time = -9999.0;

void show_status(const char* fmt, ...)
{
    static char buf[256];

    va_list argptr;
    va_start(argptr, fmt);
    vsnprintf(buf, 256, fmt, argptr);
    
    status_message = buf;
    status_time = -30.0;
}

void show_status(const std::string& text)
{
    status_message = text;
    status_time = -30.0;
}

void ui_init(SongEditor& editor, UserActionList& user_actions)
{
    // write version string
#ifdef DEBUG
    snprintf(VERSION_STR, 64, "version %s-dev / SnBx%s", APP_VERSION, FILE_VERSION);
#else
    snprintf(VERSION_STR, 64, "version %s / %s", APP_VERSION, FILE_VERSION);
#endif

    Song& song = *editor.song;
    show_about_window = false;

    // copy+paste
    // TODO use system clipboard
    user_actions.set_callback("copy", [&]() {
        Channel* channel = song.channels[editor.selected_channel];
        int pattern_id = channel->sequence[editor.selected_bar];
        if (pattern_id > 0) {
            Pattern* pattern = channel->patterns[pattern_id - 1];
            editor.note_clipboard = pattern->notes;
        }
    });

    user_actions.set_callback("paste", [&]() {
        if (!editor.note_clipboard.empty()) {
            Channel* channel = song.channels[editor.selected_channel];
            int pattern_id = channel->sequence[editor.selected_bar];
            if (pattern_id > 0) {
                Pattern* pattern = channel->patterns[pattern_id - 1];
                pattern->notes = editor.note_clipboard;
            }
        }
    });

    // undo/redo
    user_actions.set_callback("undo", [&]()
    {
        if (!editor.undo())
            show_status("Nothing to undo");
    });

    user_actions.set_callback("redo", [&]()
    {
        if (!editor.redo())
            show_status("Nothing to redo");
    });

    // song play/pause
    user_actions.set_callback("song_play_pause", [&song]() {
        song.is_playing = !song.is_playing;
    });

    // song next bar
    user_actions.set_callback("song_next_bar", [&song]() {
        song.mutex.lock();

        song.bar_position++;
        song.position += song.beats_per_bar;

        // wrap around
        if (song.bar_position >= song.length()) song.bar_position -= song.length();
        if (song.position >= song.length() * song.beats_per_bar) song.position -= song.length() * song.beats_per_bar;

        song.mutex.unlock();
    });

    // song previous bar
    user_actions.set_callback("song_prev_bar", [&song]() {
        song.mutex.lock();

        song.bar_position--;
        song.position -= song.beats_per_bar;

        // wrap around
        if (song.bar_position < 0) song.bar_position += song.length();
        if (song.position < 0) song.position += song.length() * song.beats_per_bar;

        song.mutex.unlock();
    });

    // mute selected channel
    user_actions.set_callback("mute_channel", [&]() {
        Channel* ch = editor.song->channels[editor.selected_channel];
        ch->vol_mod.mute = !ch->vol_mod.mute;
    });

    // solo selected channel
    user_actions.set_callback("solo_channel", [&]() {
        Channel* ch = song.channels[editor.selected_channel];
        ch->solo = !ch->solo;
    });

    // create new channel
    user_actions.set_callback("new_channel", [&]() {
        editor.selected_channel++;
        editor.push_change(new change::ChangeNewChannel(editor.selected_channel));
        song.insert_channel(editor.selected_channel);
    });

    // delete selected channel
    user_actions.set_callback("remove_channel", [&]() {
        if (song.channels.size() > 1)
        {
            editor.push_change(new change::ChangeRemoveChannel(editor.selected_channel, song.channels[editor.selected_channel]));
            editor.remove_channel(editor.selected_channel);
            if (editor.selected_channel > 0) editor.selected_channel--;
        }
    });

    // insert bar
    user_actions.set_callback("insert_bar", [&]() {
        editor.selected_bar++;
        
        editor.push_change(new change::ChangeInsertBar(editor.selected_bar));
        song.insert_bar(editor.selected_bar);
    });

    // insert bar before
    user_actions.set_callback("insert_bar_before", [&]() {
        editor.push_change(new change::ChangeInsertBar(editor.selected_bar));
        song.insert_bar(editor.selected_bar);
    });

    // delete bar
    user_actions.set_callback("remove_bar", [&]() {
        if (song.length() > 1)
        {
            editor.push_change(new change::ChangeRemoveBar(editor.selected_bar, &song));
            song.remove_bar(editor.selected_bar);
            if (editor.selected_bar > 0) editor.selected_bar--;
        }
    });

    // insert new pattern
    user_actions.set_callback("new_pattern", [&]() {
        int pattern_id = song.new_pattern(editor.selected_channel);
        song.channels[editor.selected_channel]->sequence[editor.selected_bar] = pattern_id + 1;
    });
}










EffectsInterfaceAction effect_rack_ui(SongEditor* editor, audiomod::EffectsRack* effects_rack, EffectsInterfaceResult* result)
{
    Song* song = editor->song;

    std::mutex& mutex = song->mutex;
    EffectsInterfaceAction action = EffectsInterfaceAction::Nothing;

    // effects
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Effects");
    ImGui::SameLine();
    if (ImGui::Button("Add##effect_add")) {
        ImGui::OpenPopup("add_effect");
    }

    if (ImGui::BeginPopup("add_effect")) {
        for (auto listing : audiomod::effects_list)
        {
            if (ImGui::Selectable(listing.name))
            {
                result->module_id = listing.id;
                action = EffectsInterfaceAction::Add;
            }
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

        static int swap_start = 0;

        for (audiomod::ModuleBase* module : effects_rack->modules) {
            ImGui::PushID(module);
            
            ImGui::Selectable(module->name.c_str(), module->show_interface);

            // record swaps
            if (ImGui::IsItemActivated())
                swap_start = i;

            if (ImGui::IsItemDeactivated())
            {
                if (swap_start != i)
                {
                    std::cout << "swapped\n";
                    action = EffectsInterfaceAction::Swapped;
                    result->swap_start = swap_start;
                    result->swap_end = i;
                }
            }

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
                result->target_index = i;
                action = EffectsInterfaceAction::Edit;
            }

            // right click to delete
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::Selectable("Remove", false)) {
                    // defer deletion until after the loop has finished
                    result->target_index = i;
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

void compute_imgui(SongEditor& editor, UserActionList& user_actions) {
    ImGuiStyle& style = ImGui::GetStyle();
    Song& song = *editor.song;
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
                editor.show_tuning_window = !editor.show_tuning_window;
            
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Preferences"))
        {
            ImGui::MenuItem("Keep Current Pattern Selected");
            ImGui::MenuItem("Hear Preview of Added Notes");
            ImGui::MenuItem("Allow Adding Notes Not in Scale");
            ImGui::MenuItem("Show Notes From All Channels");
            
            ImGui::Separator();

            if (ImGui::MenuItem("Themes..."))
            {
                editor.show_themes_window = !editor.show_themes_window;

                if (editor.show_themes_window)
                    editor.theme.scan_themes();
            }

            ImGui::MenuItem("Directories...");
            ImGui::MenuItem("MIDI Configuration...");
            
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("About..."))
                show_about_window = !show_about_window;

            if (ImGui::MenuItem("Controls..."))
                show_shortcuts_window = !show_shortcuts_window;

            if (ImGui::MenuItem("Plugin List..."))
                editor.show_plugin_list = !editor.show_plugin_list;
            
            ImGui::EndMenu();
        }

#ifdef DEBUG
        if (ImGui::BeginMenu("Dev"))
        {
            if (ImGui::MenuItem("Toggle Dear ImGUI Demo", "F1")) {
                show_demo_window = !show_demo_window;
            }

            ImGui::EndMenu();
        }
#endif

        ImGui::EndMainMenuBar();
    }

    ///////////////////
    // SONG SETTINGS //
    ///////////////////

    if (ImGui::Begin("Song Settings")) {
        // song name input
        // TODO: change detection
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
        ImGui::DragFloat("###song_tempo", &song.tempo, 1.0f, 0.0f, 5000.0f, "%.3f");
        if (song.tempo < 0) song.tempo = 0;

        { // change detection
            float prev;
            if (change_detection(editor, song.tempo, &prev)) {
                editor.push_change(new change::ChangeSongTempo(ImGui::GetItemID(), prev, song.tempo));
            }
        }
        
        // TODO: controller/mod channels
        if (ImGui::BeginPopupContextItem()) {
            ImGui::Selectable("Add to selected modulator", false);
            ImGui::EndPopup();
        }

        // max patterns per channel
        int max_patterns = song.max_patterns();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Max Patterns");

        // patterns help
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 20.0f);
            
            ImGui::TextWrapped(
                "Each channel has an individual list of patterns, and this input "
                "controls the available amount of patterns per channel. "
                "You can use this input to add more patterns, but you can also "
                "place a new note on a null pattern (pattern 0) or select Edit > New Pattern"
            );
            
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        // max patterns input
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputInt("###song_patterns", &max_patterns);
        if (max_patterns >= 1)
        {
            int old_max_patterns = song.max_patterns();
            if (max_patterns != old_max_patterns)
            {
                editor.push_change(new change::ChangeSongMaxPatterns(old_max_patterns, max_patterns, &song));
                song.set_max_patterns(max_patterns);
            }
        }

        // project notes
        // TODO: change detection?
        ImGui::Text("Project Notes");
        ImGui::InputTextMultiline_str("###project_notes", &song.project_notes, ImVec2(-1.0f, ImGui::GetTextLineHeight() * 16.0f));
    } ImGui::End();

    //////////////////////
    // CHANNEL SETTINGS //
    //////////////////////

    Channel* cur_channel = song.channels[editor.selected_channel];

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
            ImGui::SliderFloat("##channel_volume", &volume, 0.0f, 100.0f, "%.0f");
            cur_channel->vol_mod.volume = volume / 100.0f;

            // change detection
            {
                // make id
                snprintf(char_buf, 64, "chvol%i", editor.selected_channel);

                float prev, cur;
                cur = cur_channel->vol_mod.volume;
                if (change_detection(editor, cur, &prev, ImGui::GetID(char_buf))) {
                    editor.push_change(new change::ChangeChannelVolume(editor.selected_channel, prev, cur));
                }
            }
        }

        // panning slider
        {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Panning");
            ImGui::SameLine();
            ImGui::SliderFloat("##channel_panning", &cur_channel->vol_mod.panning, -1, 1, "%.2f");
            
            // change detection
            {
                // make id
                snprintf(char_buf, 64, "chpan%i", editor.selected_channel);

                float prev, cur;
                cur = cur_channel->vol_mod.panning;
                if (change_detection(editor, cur, &prev, ImGui::GetID(char_buf))) {
                    editor.push_change(new change::ChangeChannelPanning(editor.selected_channel, prev, cur));
                }
            }
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

                    // change detection
                    {
                        // make id
                        snprintf(char_buf, 64, "chout%i", editor.selected_channel);

                        int prev, cur;
                        cur = cur_channel->fx_target_idx;
                        if (change_detection(editor, cur, &prev, ImGui::GetID(char_buf))) {
                            editor.push_change(new change::ChangeChannelOutput(editor.selected_channel, prev, cur));
                        }
                    }
                }

                ImGui::EndCombo();

            }
        }

        ImGui::PopItemWidth();
        ImGui::NewLine();

        // load instrument
        ImGui::Text("Instrument: %s", cur_channel->synth_mod->name.c_str());
        if (ImGui::Button("Load...", ImVec2(ImGui::GetWindowSize().x / -2.0f, 0.0f)))
        {
            ImGui::OpenPopup("load_instrument");
        }
        ImGui::SameLine();

        if (ImGui::BeginPopup("load_instrument")) {
            for (auto listing : audiomod::instruments_list)
            {
                if (ImGui::Selectable(listing.name))
                {
                    
                }
            }

            ImGui::EndPopup();
        }

        // edit loaded instrument
        if (ImGui::Button("Edit...", ImVec2(-1.0f, 0.0f)))
        {
            editor.toggle_module_interface(cur_channel->synth_mod);
        }

        EffectsInterfaceResult result;
        switch (effect_rack_ui(&editor, &cur_channel->effects_rack, &result))
        {
            case EffectsInterfaceAction::Add: {
                song.mutex.lock();

                audiomod::ModuleBase* mod = audiomod::create_module(result.module_id, &song);
                mod->parent_name = cur_channel->name;
                cur_channel->effects_rack.insert(mod);

                // register change
                editor.push_change(new change::ChangeAddEffect(
                    editor.selected_channel,
                    change::FXRackTargetType::TargetChannel,
                    result.module_id
                ));

                song.mutex.unlock();
                break;
            }

            case EffectsInterfaceAction::Edit:
                editor.toggle_module_interface(cur_channel->effects_rack.modules[result.target_index]);
                break;

            case EffectsInterfaceAction::Delete: {
                // delete the selected module
                song.mutex.lock();

                audiomod::ModuleBase* mod = cur_channel->effects_rack.remove(result.target_index);
                if (mod != nullptr) {
                    // register change
                    editor.push_change(new change::ChangeRemoveEffect(
                        editor.selected_channel,
                        change::FXRackTargetType::TargetChannel,
                        result.target_index,
                        mod
                    ));

                    editor.hide_module_interface(mod);
                    delete mod;
                }

                song.mutex.unlock();
                break;
            }

            case EffectsInterfaceAction::Swapped:
                editor.push_change(new change::ChangeSwapEffect(
                    editor.selected_channel,
                    change::FXRackTargetType::TargetChannel,
                    result.swap_start,
                    result.swap_end
                ));

                break;

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
    render_track_editor(editor);


    ////////////////////
    // PATTERN EDITOR //
    ////////////////////
    render_pattern_editor(editor);

    ////////////////////
    //    FX MIXER    //
    ////////////////////
    if (ImGui::Begin("FX Mixer"))
    {
        int i = 0;

        float frame_width = ImGui::CalcTextSize("MMMMMMMMMMMMMMMM").x + style.FramePadding.x;
        
        float mute_btn_size = ImGui::CalcTextSize("M").x + style.FramePadding.x * 2.0f;
        float solo_btn_size = ImGui::CalcTextSize("S").x + style.FramePadding.x * 2.0f;

        audiomod::FXBus* bus_to_delete = nullptr;

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

            // right click to remove
            if (i != 0 && ImGui::BeginPopupContextItem())
            {
                if (ImGui::Selectable("Remove", false))
                    bus_to_delete = bus;

                ImGui::EndPopup();
            }

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

        // delete requested bus
        if (bus_to_delete)
        {
            song.mutex.lock();
            song.delete_fx_bus(bus_to_delete);
            song.mutex.unlock();
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
    for (size_t i = 0; i < editor.mod_interfaces.size();)
    {
        auto mod = editor.mod_interfaces[i];
        
        if (mod->render_interface()) i++;
        else {
            editor.mod_interfaces.erase(editor.mod_interfaces.begin() + i);
        }
    }

    // render fx interfaces
    int i = 0;
    audiomod::FXBus* bus_to_delete = nullptr;

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
                if (ImGui::Button("Remove"))
                    ImGui::OpenPopup("Remove");

                if (ImGui::BeginPopup("Remove")) {
                    if (ImGui::Selectable("Confirm?"))
                        bus_to_delete = fx_bus;

                    ImGui::EndPopup();
                }

                // show help explaining alternative method to remove buses
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
                    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 20.0f);
                    
                    ImGui::TextWrapped(
                        "You can also remove FX buses by right-clicking on them "
                        "in the bus list. "
                    );
                    
                    ImGui::PopTextWrapPos();
                    ImGui::EndTooltip();
                }
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

            EffectsInterfaceResult result;
            switch (effect_rack_ui(&editor, &fx_bus->rack, &result))
            {
                case EffectsInterfaceAction::Add: {
                    song.mutex.lock();
                    
                    audiomod::ModuleBase* mod = audiomod::create_module(result.module_id, &song);
                    mod->parent_name = fx_bus->name;
                    fx_bus->insert(mod);

                    // register change
                    editor.push_change(new change::ChangeAddEffect(
                        i,
                        change::FXRackTargetType::TargetFXBus,
                        result.module_id
                    ));

                    song.mutex.unlock();
                    break;
                }

                case EffectsInterfaceAction::Edit:
                    editor.toggle_module_interface(fx_bus->rack.modules[result.target_index]);
                    break;

                case EffectsInterfaceAction::Delete: {
                    // delete the selected module
                    song.mutex.lock();

                    audiomod::ModuleBase* mod = fx_bus->rack.remove(result.target_index);
                    if (mod != nullptr) {
                        // register change
                        editor.push_change(new change::ChangeRemoveEffect(
                            i,
                            change::FXRackTargetType::TargetFXBus,
                            result.target_index,
                            mod
                        ));

                        editor.hide_module_interface(mod);
                        delete mod;
                    }

                    song.mutex.unlock();
                    break;
                }

                case EffectsInterfaceAction::Swapped:
                    editor.push_change(new change::ChangeSwapEffect(
                        i,
                        change::FXRackTargetType::TargetFXBus,
                        result.swap_start,
                        result.swap_end
                    ));
                    
                    break;

                case EffectsInterfaceAction::Nothing: break;
            }
        }

        ImGui::End();

        i++;
    }

    if (bus_to_delete)
        song.delete_fx_bus(bus_to_delete);
    
    // tunings window
    if (editor.show_tuning_window) {
        if (ImGui::Begin("Tunings", &editor.show_tuning_window, ImGuiWindowFlags_NoDocking))
        {
            if (ImGui::Button("Load"))
                user_actions.fire("load_tuning");

            if (ImGui::BeginChild("list", ImVec2(ImGui::GetContentRegionAvail().x * 0.4f, -1.0f)))
            {
                int i = 0;
                int index_to_delete = -1;

                for (Tuning* tuning : song.tunings)
                {
                    ImGui::PushID(i);

                    if (ImGui::Selectable(tuning->name.c_str(), i == song.selected_tuning))
                        song.selected_tuning = i;

                    // right-click to remove (except 12edo, that can't be deleted)
                    if (i > 0 && ImGui::BeginPopupContextItem())
                    {
                        if (ImGui::Selectable("Remove"))
                            index_to_delete = i;
                        ImGui::EndPopup();
                    }

                    if (song.selected_tuning == i) ImGui::SetItemDefaultFocus();

                    ImGui::PopID();
                    i++;
                }

                // if a deletion was requested
                if (index_to_delete >= 0) {
                    if (song.selected_tuning == index_to_delete) {
                        song.selected_tuning--;
                    }

                    song.tunings.erase(song.tunings.begin() + index_to_delete);
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

            // file extension info (exists to tell the user if they can apply kbm to the tuning)
            if (selected_tuning->is_12edo)
                ImGui::NewLine();
            else if (selected_tuning->scl_import == nullptr)
                ImGui::Text("TUN file");
            else
            {
                ImGui::Text("SCL file");

                // kbm import help
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
                    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 15.0f);
                    
                    ImGui::Text("Loading a .kbm file while an .scl tuning is selected will apply the keyboard mapping to the scale.");

                    ImGui::PopTextWrapPos();
                    ImGui::EndTooltip();
                }
            }

            ImGui::EndGroup();
        } ImGui::End();
    }

    // themes window
    if (editor.show_themes_window) {
        if (ImGui::Begin("Themes", &editor.show_themes_window, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (ImGui::BeginMenuBar())
            {
                if (ImGui::BeginMenu("File"))
                {
                    ImGui::Selectable("Show Theme Editor...");
                    ImGui::Selectable("Export Theme to File...");
                    ImGui::EndMenu();
                }

                ImGui::EndMenuBar();
            }

            for (const std::string& name : editor.theme.get_themes_list())
            {
                if (ImGui::Selectable(name.c_str(), name == editor.theme.name()))
                {
                    editor.theme.load(name);
                    editor.theme.set_imgui_colors();
                }
            }
        } ImGui::End();
    }

    // Info panel //
    ImGuiIO& io = ImGui::GetIO();

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
        ImGui::PushTextWrapPos(ImGui::GetTextLineHeight() * 38.0f);
        
        // draw logo
        if (logo_texture)
        {
            ImGui::SetCursorPosX((window_width - (float)logo_width) / 2.0f);
            ImGui::Image((void*)(intptr_t)logo_texture, ImVec2(logo_width, logo_height));
        }
        
        // app version / file format version
        ImGui::SetCursorPosX((window_width - ImGui::CalcTextSize(VERSION_STR).x) / 2.0f);
        ImGui::Text("%s", VERSION_STR);

        ImGui::NewLine();
        ImGui::TextWrapped("The interface of this application is heavily influenced by John Nesky's BeepBox.");
        ImGui::NewLine();
        ImGui::TextWrapped("Open-source libraries:");

        ImGui::Bullet();
        ImGui::TextWrapped("PortAudio: http://www.portaudio.com/");
        ImGui::Bullet();
        ImGui::TextWrapped("Dear ImGui: https://www.dearimgui.com/");
        ImGui::Bullet();
        ImGui::TextWrapped("glfw: https://www.glfw.org/");
        ImGui::Bullet();
        ImGui::TextWrapped("AnaMark tuning library: https://github.com/zardini123/AnaMark-Tuning-Library");
        ImGui::Bullet();
        ImGui::TextWrapped("Surge Synth tuning library: https://surge-synth-team.org/tuning-library/");
        ImGui::Bullet();
        ImGui::TextWrapped("nativefiledialog: https://github.com/mlabbe/nativefiledialog");
        ImGui::Bullet();
        ImGui::TextWrapped("stb_image: https://github.com/nothings/stb");
        ImGui::End();
    }

    // shortcuts window
    if (show_shortcuts_window && ImGui::Begin(
        "Keyboard Controls",
        &show_shortcuts_window,
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize
    ))
    {
        ImGui::PushTextWrapPos(ImGui::GetTextLineHeight() * 24.0f);

        ImGui::TextWrapped(
            "Most keyboard controls are shown in the File and Edit menus, but not all."
            " The following is a list of keyboard controls not documented elsewhere:"
            );

        ImGui::Bullet();
        ImGui::TextWrapped("Numbers 0-9: Type in index of selected pattern");
        ImGui::Bullet();
        ImGui::TextWrapped("Arrow keys: Move cursor in track editor");
        ImGui::Bullet();
        ImGui::TextWrapped("Ctrl+M: Mute selected channel");
        ImGui::Bullet();
        ImGui::TextWrapped("Shift+M: Solo selected channel");
        ImGui::Bullet();
        ImGui::TextWrapped("Space: Play/Pause song");
        ImGui::Bullet();
        ImGui::TextWrapped("] (Right Bracket): Move playhead right");
        ImGui::Bullet();
        ImGui::TextWrapped("[ (Left Bracket): Move playhead left");

        ImGui::End();
    }

    // plugin list
    static int selected_plugin = 0;

    if (editor.show_plugin_list && ImGui::Begin(
        "Plugin List",
        &editor.show_plugin_list,
        ImGuiWindowFlags_NoDocking
    ))
    {
        auto& plugin_list = editor.plugin_manager.get_plugin_data();

        selected_plugin = -1;

        if (ImGui::BeginChild("list", ImVec2(ImGui::GetContentRegionAvail().x * 0.4f, -1.0f)))
        {
            int i = 0;
            for (auto& plugin_data : plugin_list)
            {
                ImGui::PushID(i);
                ImGui::Selectable(plugin_data.name.c_str(), i == selected_plugin);
                if (ImGui::IsItemHovered())
                    selected_plugin = i;

                ImGui::PopID();
                i++;
            }
        }

        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginGroup();

        if (selected_plugin >= 0)
        {
            auto& plugin_data = plugin_list[selected_plugin];

            ImGui::Text("File: %s", plugin_data.file_path.c_str());
            ImGui::Separator();
            ImGui::Text("Author: %s", plugin_data.author.c_str());
            ImGui::Separator();
            ImGui::Text("Copyright: %s", plugin_data.copyright.c_str());
            ImGui::Separator();

            const char* plugin_type;

            switch (plugin_data.type)
            {
                case plugins::PluginType::Ladspa:
                    plugin_type = "LADSPA";
                    break;

                case plugins::PluginType::Lv2:
                    plugin_type = "LV2";
                    break;

                case plugins::PluginType::Clap:
                    plugin_type = "CLAP";
                    break;

                case plugins::PluginType::Vst:
                    plugin_type = "VST2";
                    break;
            }

            ImGui::Text("Is Instrument?: %s", plugin_data.is_instrument ? "Yes": "No");
            ImGui::Separator();
            ImGui::Text("Type: %s", plugin_type);
        }

        ImGui::EndGroup();
        ImGui::End();
    }

    // show status info as an overlay
    if (status_time == -30.0) status_time = ImGui::GetTime();

    if (ImGui::GetTime() < status_time + 5.0) {
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

    // run all scheduled actions
    // (run them last so that the song pointer doesn't change mid-function
    // and cause memory errors)
    for (const char* action_name : deferred_actions)
    {
        user_actions.fire(action_name);
    }
}