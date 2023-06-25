#include <cassert>
#include <cstdio>
#include <math.h>
#include <iostream>
#include <string>
#include <imgui.h>
#include <nfd.h>
#include "../audio.h"
#include "change_history.h"
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

std::string file_browser(FileBrowserMode mode, const std::string &filter_list, const std::string &default_path)
{
    std::string result_path("");
    nfdchar_t* out_path;

    nfdresult_t result;
    
    if (mode == FileBrowserMode::Save)
        result = NFD_SaveDialog(filter_list.c_str(), default_path.c_str(), &out_path);
    else if (mode == FileBrowserMode::Open)
        result = NFD_OpenDialog(filter_list.c_str(), default_path.c_str(), &out_path);
    else if (mode == FileBrowserMode::Directory)
        result = NFD_PickFolder(default_path.c_str(), &out_path);
    else
        throw std::runtime_error("invalid FileBrowserMode " + std::to_string((int)mode));
    
    switch (result)
    {
        case NFD_OKAY:
            result_path = out_path;
            break;

        case NFD_CANCEL:
            break; 

        case NFD_ERROR:
            std::cerr << "error: " << NFD_GetError() << "\n";
            break;
    }

    return result_path;
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

void ui_init(SongEditor& editor)
{
    UserActionList& user_actions = editor.ui_actions;

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
        ImGui::SeparatorText("Built-in");

        // display built-in effects
        for (auto listing : audiomod::effects_list)
        {
            if (ImGui::Selectable(listing.name))
            {
                result->module_id = listing.id;
                action = EffectsInterfaceAction::Add;
            }
        }

        // display effect plugins
        ImGui::SeparatorText("Plugins");

        for (auto& plugin_data : editor->plugin_manager.get_plugin_data())
        {
            if (ImGui::Selectable(plugin_data.name.c_str()))
            {
                result->module_id = plugin_data.id.c_str();
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

void compute_imgui(SongEditor& editor) {
    ImGuiStyle& style = ImGui::GetStyle();
    Song& song = *editor.song;
    UserActionList& user_actions = editor.ui_actions;
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

            if (ImGui::MenuItem("Directories..."))
            {
                editor.show_dir_window = !editor.show_dir_window;
            }
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

    render_song_settings(editor);
    render_channel_settings(editor);
    render_track_editor(editor);
    render_pattern_editor(editor);
    render_fx_mixer(editor);  

    render_tunings_window(editor);  
    render_plugin_list(editor);
    render_themes_window(editor);
    render_directories_window(editor);

    // render module interfaces
    for (size_t i = 0; i < editor.mod_interfaces.size();)
    {
        auto mod = editor.mod_interfaces[i];
        
        if (mod->render_interface()) i++;
        else {
            editor.mod_interfaces.erase(editor.mod_interfaces.begin() + i);
        }
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