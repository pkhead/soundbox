#include <cassert>
#include <cstdio>
#include <math.h>
#include <iostream>
#include <string>
#include <imgui.h>
#include <nfd.h>
#include <stb_image.h>
#include <glad/gl.h>
#include "../audio.h"
#include "../editor/change_history.h"
#include "../modules/modules.h"
#include "../song.h"
#include "../editor/editor.h"
#include "../editor/theme.h"
#include "ui.h"
#include "../util.h"

using namespace ui;

constexpr char APP_VERSION[] = "0.1.0";
constexpr char FILE_VERSION[] = "0001";
char VERSION_STR[64];

GLuint logo_texture;
int logo_width, logo_height;

ImU32 hex_color(const char* str)
{
    int r, g, b;
    sscanf(str, "%02x%02x%02x", &r, &g, &b);
    return IM_RGB32(r, g, b);    
}

void ui::push_btn_disabled(ImGuiStyle& style, bool is_disabled)
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

void ui::pop_btn_disabled()
{
    ImGui::PopStyleColor(3);
}

std::string ui::file_browser(FileBrowserMode mode, const std::string &filter_list, const std::string &default_path)
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














bool ui::show_demo_window = false;
bool show_about_window = false;
bool show_shortcuts_window = false;

std::string status_message;
double status_time = -9999.0;

void ui::show_status(const char* fmt, ...)
{
    static char buf[256];

    va_list argptr;
    va_start(argptr, fmt);
    vsnprintf(buf, 256, fmt, argptr);
    
    status_message = buf;
    status_time = -30.0;
}

void ui::show_status(const std::string& text)
{
    status_message = text;
    status_time = -30.0;
}

void ui::ui_init(SongEditor& editor)
{
    // load logo
    {
        logo_texture = 0;
        logo_width = 0;
        logo_height = 0;

        uint8_t* img_data = stbi_load("logo.png", &logo_width, &logo_height, NULL, 4);
        if (img_data)
        {
            // create opengl texture
            glGenTextures(1, &logo_texture);
            glBindTexture(GL_TEXTURE_2D, logo_texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            // upload pictures into texture
#ifdef GL_UNPACK_ROW_LENGTH
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RGBA,
                logo_width,
                logo_height,
                0,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                img_data
            );

            // we are done
            stbi_image_free(img_data);
        }
    }

    UserActionList& user_actions = editor.ui_actions;

    // write version string
#ifdef DEBUG
    snprintf(VERSION_STR, 64, "version %s-dev / SnBx%s", APP_VERSION, FILE_VERSION);
#else
    snprintf(VERSION_STR, 64, "version %s / SnBx%s", APP_VERSION, FILE_VERSION);
#endif

    Song& song = *editor.song;
    show_about_window = false;

    // copy+paste
    // TODO use system clipboard
    user_actions.set_callback("copy", [&]() {
        auto& channel = song.channels[editor.selected_channel];
        int pattern_id = channel->sequence[editor.selected_bar];
        if (pattern_id > 0) {
            auto& pattern = channel->patterns[pattern_id - 1];
            editor.note_clipboard = pattern->notes;
        }
    });

    user_actions.set_callback("paste", [&]() {
        if (!editor.note_clipboard.empty()) {
            auto& channel = song.channels[editor.selected_channel];
            int pattern_id = channel->sequence[editor.selected_bar];
            if (pattern_id > 0) {
                auto& pattern = channel->patterns[pattern_id - 1];
                pattern->notes = editor.note_clipboard;
            }
        }
    });

    // TODO: add undo/redo
    auto move_notes = [&editor](int steps, int sign) {
        auto& channel = editor.song->channels[editor.selected_channel];
        int pattern_id = channel->sequence[editor.selected_bar];
        if (pattern_id > 0) {
            auto& pattern = channel->patterns[pattern_id - 1];
            
            for (auto& note : pattern->notes)
            {
                for (int i = 0; i < steps; i++)
                {
                    if (editor.song->is_note_playable(note.key + sign))
                        note.key += sign;
                    
                }

                note.key = std::clamp(note.key, 0, 96);
            }
        }
    };

    user_actions.set_callback("move_notes_up", [move_notes]() {
        move_notes(1, 1);
    });

    user_actions.set_callback("move_notes_down", [move_notes]() {
        move_notes(1, -1);
    });

    user_actions.set_callback("move_notes_up_oct", [move_notes]() {
        move_notes(12, 1);
    });

    user_actions.set_callback("move_notes_down_oct", [move_notes]() {
        move_notes(12, -1);
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
    user_actions.set_callback("mute_channel", [&]() {
        auto& ch = editor.song->channels[editor.selected_channel];
        auto& vol_mod = ch->vol_mod->module<audiomod::VolumeModule>();
        vol_mod.mute = !vol_mod.mute;
    });

    // solo selected channel
    user_actions.set_callback("solo_channel", [&]() {
        auto& ch = song.channels[editor.selected_channel];
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
            editor.push_change(new change::ChangeRemoveBar(editor.selected_bar, *editor.song));
            song.remove_bar(editor.selected_bar);
            if (editor.selected_bar > 0) editor.selected_bar--;
        }
    });

    // insert new pattern
    user_actions.set_callback("new_pattern", [&]() {
        int pattern_id = song.new_pattern(editor.selected_channel);
        song.channels[editor.selected_channel]->sequence[editor.selected_bar] = pattern_id + 1;
    });

    // move playhead to cursor
    user_actions.set_callback("goto_cursor", [&]() {
        song.bar_position = editor.selected_bar;
    });

    // move playhead to start
    user_actions.set_callback("goto_start", [&]() {
        song.bar_position = 0;
    });
}






static bool str_search(const std::string& str, const char* query)
{
    // a C string is empty if and only if the first character is NULL
    if (query[0] == 0) return true;

    // convert input strings to lowercase
    size_t query_size = strlen(query);
    std::string str_low;
    std::string query_low;
    str_low.reserve(str.size());
    query_low.reserve(query_size);

    for (size_t i = 0; i < str.size(); i++)
        str_low.push_back(std::tolower(str[i]));    

    for (size_t i = 0; i < query_size; i++)
        query_low.push_back(std::tolower(query[i]));

    // perform search
    return str_low.find(query_low) != std::string::npos;
}

const char* ui::module_selection_popup(SongEditor& editor, bool instruments)
{
    static char search_query[64];
    static bool force_instruments = false;

    if (ImGui::IsWindowAppearing()) {
        // clear search query
        search_query[0] = 0;
        
        // focus on search bar
        ImGui::SetKeyboardFocusHere();
    }

    // if enter is pressed, select the only displayed module
    // if there is more than one module displayed, this does nothing
    bool enter_pressed = ImGui::InputTextWithHint(
        "##search",
        "Search...",
        search_query, 64,
        ImGuiInputTextFlags_EnterReturnsTrue
    );
    int displayed = 0;
    const char* displayed_module_id = nullptr;

    // if only showing effects, user can choose to show instrument plugins
    // this is useful for when the user is using a module that emits
    // MIDI output, and the synthesizer would then need to be in the
    // effects rack
    if (!instruments) {
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Instruments");
        ImGui::SameLine();
        ImGui::Checkbox("##show_instruments", &force_instruments);
    }
    const char* new_module_id = nullptr;

    struct selectable {
        const char* name;
        const char* id;
    };

    std::vector<selectable> builtin;
    std::vector<selectable> plugins;

    // display built-in instruments
    if (instruments || force_instruments)
    {
        for (auto listing : audiomod::instruments_list)
        {
            if ( str_search(listing.name, search_query) ) {
                builtin.push_back({ listing.name, listing.id });
            }
        }
    }

    // or display built in effects
    if (!instruments) {
        for (auto listing : audiomod::effects_list)
        {
            if ( str_search(listing.name, search_query) ) {
                builtin.push_back({ listing.name, listing.id });
            }
        }
    }

    // display effect plugins
    //ImGui::SeparatorText("Plugins");

    for (auto& plugin_data : editor.plugin_manager.get_plugin_data())
    {
        if (
            // if instruments is true, only show plugin if it is an instrument
            // otherwise, show all plugins
            (instruments == plugin_data.is_instrument || force_instruments) &&
            // search query
            ( str_search(plugin_data.name, search_query) )
        ) {
            plugins.push_back({ plugin_data.name.c_str(), plugin_data.id.c_str() });
        }
    }

    // calculate size of popup
    float size_y = 
        (builtin.size() + plugins.size()) * ImGui::GetTextLineHeightWithSpacing() +
        ImGui::GetFrameHeightWithSpacing() * 2;
    
    ImGui::BeginChild("###scrollarea", ImVec2(0.0f,
        min(size_y, ImGui::GetTextLineHeight() * 30.0f)
    ));

    ImGui::SeparatorText("Built-in");

    for (auto [ name, id ] : builtin) {
        displayed_module_id = id;
        displayed++;

        if (ImGui::Selectable(name)) {
            new_module_id = id;
            ImGui::CloseCurrentPopup();
        }
    }

    ImGui::SeparatorText("Plugins");

    for (auto [ name, id ] : plugins) {
        displayed_module_id = id;
        displayed++;

        if (ImGui::Selectable(name)) {
            new_module_id = id;
            ImGui::CloseCurrentPopup();
        }
    }

    ImGui::EndChild();
    
    if (enter_pressed && displayed == 1) {
        new_module_id = displayed_module_id;
        std::cout << "aa\n";
        ImGui::CloseCurrentPopup();
    }

    return new_module_id;
}









EffectsInterfaceAction ui::effect_rack_ui(
    SongEditor* editor,
    audiomod::EffectsRack* effects_rack,
    EffectsInterfaceResult* result,
    bool swap_instrument
) {
    auto& song = editor->song;

    EffectsInterfaceAction action = EffectsInterfaceAction::Nothing;

    // effects
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Effects");
    ImGui::SameLine();
    if (ImGui::Button("Add##effect_add")) {
        ImGui::OpenPopup("add_effect");
    }

    if (ImGui::BeginPopup("add_effect")) {
        const char* mod_id = module_selection_popup(*editor, false);
        ImGui::EndPopup();

        if (mod_id) {
            result->module_id = mod_id;
            action = EffectsInterfaceAction::Add;
        }
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

        for (auto& mnode : effects_rack->modules) {
            audiomod::ModuleBase& module = mnode->module();
            ImGui::PushID(&module);
            
            ImGui::Selectable(module.name.c_str(), module.interface_shown());

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
                    auto m = effects_rack->remove(min);
                    effects_rack->insert(m, min + 1);
                    ImGui::ResetMouseDragDelta();
                }
            }

            // double click to edit
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                result->target_index = i;
                action = EffectsInterfaceAction::Edit;
            }

            // right click to delete
            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::Selectable("Remove")) {
                    // defer deletion until after the loop has finished
                    result->target_index = i;
                    action = EffectsInterfaceAction::Delete;
                }

                if (swap_instrument)
                {
                    if (audiomod::is_module_instrument(module.id, editor->plugin_manager)) {
                        if (ImGui::Selectable("Set as Instrument")) {
                            result->target_index = i;
                            action = EffectsInterfaceAction::SwapInstrument;
                        }
                    } else {
                        ImGui::TextDisabled("Set as Instrument");
                    }
                }

                ImGui::EndPopup();
            }

            ImGui::PopID();
            i++;
        }
    }

    return action;
}

bool show_unsaved_work_prompt = false;
std::function<void()> unsaved_work_callback;

void ui::prompt_unsaved_work(std::function<void ()> callback)
{
    show_unsaved_work_prompt = true;
    unsaved_work_callback = callback;
}

// menu item helper
#define MENU_ITEM(label, action_name) \
    if (ImGui::MenuItem(label, user_actions.combo_str(action_name))) \
        deferred_actions.push_back(action_name)

void ui::compute_imgui(SongEditor& editor) {
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
            MENU_ITEM("Playhead To Cursor", "goto_cursor");
            MENU_ITEM("Playhead To Start", "goto_start");

            ImGui::Separator();

            MENU_ITEM("Copy Pattern", "copy");
            MENU_ITEM("Paste Pattern", "paste");
            MENU_ITEM("Paste Pattern Numbers", "paste_pattern_numbers");
            MENU_ITEM("Move Notes Up", "move_notes_up");
            MENU_ITEM("Move Notes Down", "move_notes_down");

            ImGui::Separator();

            MENU_ITEM("Insert Bar", "insert_bar");
            MENU_ITEM("Insert Bar Before", "insert_bar_before");
            MENU_ITEM("Delete Bar", "remove_bar");
            MENU_ITEM("Duplicate Reused Patterns", "duplicate_patterns");

            ImGui::Separator();

            MENU_ITEM("New Channel", "new_channel");
            MENU_ITEM("Delete Channel", "remove_channel");

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
            if (ImGui::MenuItem("Keep Current Pattern Selected", nullptr, editor.follow_playhead))
                editor.follow_playhead = !editor.follow_playhead;

            if (ImGui::MenuItem("Hear Preview of Added Notes", nullptr, editor.note_preview))
                editor.note_preview = !editor.note_preview;

            if (ImGui::MenuItem("Show Notes From All Channels", nullptr, editor.show_all_channels))
                editor.show_all_channels = !editor.show_all_channels;
            
            ImGui::Separator();

            if (ImGui::MenuItem("Themes..."))
            {
                editor.show_themes_window = !editor.show_themes_window;

                if (editor.show_themes_window)
                    editor.theme.scan_themes(editor.get_data_directory()/"themes");
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
    render_export_window(editor);

    // render module interfaces
    for (size_t i = 0; i < editor.mod_interfaces.size();)
    {
        auto mod = editor.mod_interfaces[i];
        
        if (mod->module().render_interface()) i++;
        else {
            mod->module().hide_interface();
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

    // unsaved work prompt
    // show new prompt
    if (show_unsaved_work_prompt) {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::OpenPopup("Unsaved work##unsaved_work");
        show_unsaved_work_prompt = false;
    }

    if (ImGui::BeginPopupModal("Unsaved work##unsaved_work", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::Text("Do you want to save your work before continuing?");
        ImGui::NewLine();
        
        if (ImGui::Button("Yes")) {
            ImGui::CloseCurrentPopup();
            
            // if song was able to successfully be saved, then continue
            if (editor.save_song()) unsaved_work_callback();
        }

        ImGui::SameLine();
        if (ImGui::Button("No")) {
            ImGui::CloseCurrentPopup();
            unsaved_work_callback();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
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
