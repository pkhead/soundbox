#include <memory>
#include <sstream>
#include "app.hpp"
#include "imgui.h"
#include "../modules/internal/host.hpp"
#include "shortcuts.hpp"

using namespace sbox;

Application *Application::instance = nullptr;

Application::Application()
{
    assert(Application::instance == nullptr);
    Application::instance = this;
    running = true;

    _show_imgui_demo_window = false;
    _audio_engine.register_host(std::make_unique<hosts::internal::InternalModuleHost>());
    _song = std::make_unique<Song>(4, 4, 4, _audio_engine);
    _song_editor = std::make_unique<SongEditor>(*_song, shortcut_ctx);

    theme.set_imgui_colors();
}

Application::~Application()
{
    Application::instance = nullptr;
}

void Application::request_close()
{
    running = false;
}

void Application::update(float dt)
{
    shortcut_ctx.update();
    draw_interface();

    handle_shortcuts();
    _audio_engine.update();
}

void Application::handle_shortcuts()
{
    if (shortcut_ctx.is_activated(ShortcutID::PLAY_PAUSE))
    {
        logger::log_debug("toggle play/pause");
        _song->is_playing = !_song->is_playing;
    }
}

/*#define MENU_ITEM(label, action_name) \
    if (ImGui::MenuItem(label, user_actions.combo_str(action_name))) \
        deferred_actions.push_back(action_name)*/

#define MENU_ITEM(label, action_name) ImGui::MenuItem(label);

void Application::draw_interface()
{
    ImGui::DockSpaceOverViewport();
    
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

            //if (ImGui::MenuItem("Quit", "Alt+F4"))
            //    user_actions.fire("quit");
            
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

            //if (ImGui::MenuItem("Tuning..."))
            //    editor.show_tuning_window = !editor.show_tuning_window;
            
            ImGui::EndMenu();
        }

        /*if (ImGui::BeginMenu("Preferences"))
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
        }*/

        if (ImGui::BeginMenu("Help"))
        {
            /*if (ImGui::MenuItem("About..."))
                show_about_window = !show_about_window;

            if (ImGui::MenuItem("Controls..."))
                show_shortcuts_window = !show_shortcuts_window;

            if (ImGui::MenuItem("Plugin List..."))
                editor.show_plugin_list = !editor.show_plugin_list;*/
            
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    _song_editor->draw();

    if (ImGui::IsKeyPressed(ImGuiKey_F1))
    {
        _show_imgui_demo_window = !_show_imgui_demo_window;
    }

    if (_show_imgui_demo_window)
        ImGui::ShowDemoWindow(&_show_imgui_demo_window);
}







///////////////
// SHORTCUTS //
///////////////

ShortcutContext::ShortcutContext()
{
    bind("Quit", ShortcutID::QUIT, ModKeys::ALT, ImGuiKey_F4);

    bind("Play/Pause", ShortcutID::PLAY_PAUSE, ModKeys::NONE, ImGuiKey_Space);
    bind("Playhead Next", ShortcutID::PLAYHEAD_NEXT, ModKeys::NONE, ImGuiKey_RightBracket);
    bind("Playhead Previous", ShortcutID::PLAYHEAD_PREV, ModKeys::NONE, ImGuiKey_LeftBracket);

    bind("Cursor Left", ShortcutID::CURSOR_LEFT, ModKeys::NONE, ImGuiKey_LeftArrow, true);
    bind("Cursor Up", ShortcutID::CURSOR_UP, ModKeys::NONE, ImGuiKey_UpArrow, true);
    bind("Cursor Right", ShortcutID::CURSOR_RIGHT, ModKeys::NONE, ImGuiKey_RightArrow, true);
    bind("Cursor Down", ShortcutID::CURSOR_DOWN, ModKeys::NONE, ImGuiKey_DownArrow, true);
}

void ShortcutContext::bind(const std::string &name, ShortcutID id, ModKeys mods, ImGuiKey key, bool allow_repeat)
{
    if (key == ImGuiKey_Backspace) key = ImGuiKey_Delete;

    Binding binding = Binding
    {
        .id = id,
        .name = name,
        .shortcut_string = generate_shortcut_string(mods, key),
        .key = key,
        .mods = mods,

        .is_activated = false,
        .is_deactivated = false,
        .is_active = false,
        .allow_repeat = allow_repeat
    };

    _key_shortcuts[id] = binding;
}

#define HAS_BIT(a, b) (((a) & (b)) != 0)

static int imgui_mod_flags(ModKeys m)
{
    int ret;
    if (HAS_BIT((int)m, (int)ModKeys::CTRL)) ret |= ImGuiMod_Ctrl;
    if (HAS_BIT((int)m, (int)ModKeys::SHIFT)) ret |= ImGuiMod_Shift;
    if (HAS_BIT((int)m, (int)ModKeys::ALT)) ret |= ImGuiMod_Alt;
    return ret;
}

bool ShortcutContext::is_key_pressed(const Binding &binding)
{
    if (binding.key == ImGuiKey_None) return false;

    bool kp;

    // delete/backspace will do the same thing
    if (binding.key == ImGuiKey_Delete)
        kp = ImGui::IsKeyPressed(ImGuiKey_Delete, binding.allow_repeat) || ImGui::IsKeyPressed(ImGuiKey_Backspace, binding.allow_repeat);
    else
        kp = ImGui::IsKeyPressed(binding.key, binding.allow_repeat);
    
    int mod_flags = imgui_mod_flags(binding.mods);
    return kp &&
    (HAS_BIT(mod_flags, ImGuiMod_Ctrl) == ImGui::IsKeyDown(ImGuiKey_ModCtrl)) &&
    (HAS_BIT(mod_flags, ImGuiMod_Shift) == ImGui::IsKeyDown(ImGuiKey_ModShift)) &&
    (HAS_BIT(mod_flags, ImGuiMod_Alt) == ImGui::IsKeyDown(ImGuiKey_ModAlt)) &&
    (HAS_BIT(mod_flags, ImGuiMod_Super) == ImGui::IsKeyDown(ImGuiKey_ModSuper));
}

bool ShortcutContext::is_key_down(const Binding &binding)
{
    if (binding.key == ImGuiKey_None) return false;

    bool kp;

    // delete/backspace will do the same thing
    if (binding.key == ImGuiKey_Delete)
        kp = ImGui::IsKeyDown(ImGuiKey_Delete) || ImGui::IsKeyDown(ImGuiKey_Backspace);
    else
        kp = ImGui::IsKeyDown(binding.key);
    
    int mod_flags = imgui_mod_flags(binding.mods);
    return kp &&
    (HAS_BIT(mod_flags, ImGuiMod_Ctrl) == ImGui::IsKeyDown(ImGuiKey_ModCtrl)) &&
    (HAS_BIT(mod_flags, ImGuiMod_Shift) == ImGui::IsKeyDown(ImGuiKey_ModShift)) &&
    (HAS_BIT(mod_flags, ImGuiMod_Alt) == ImGui::IsKeyDown(ImGuiKey_ModAlt)) &&
    (HAS_BIT(mod_flags, ImGuiMod_Super) == ImGui::IsKeyDown(ImGuiKey_ModSuper));
}

void ShortcutContext::update()
{
    bool input_disabled = ImGui::GetIO().WantTextInput;

    for (auto& [ _, binding ] : _key_shortcuts)
    {
        binding.is_activated = false;

        if (is_key_pressed(binding) && !input_disabled)
        {
            binding.is_activated = true;
            binding.is_active = true;
        }

        binding.is_deactivated = false;
        if (binding.is_active && !is_key_down(binding))
        {
            binding.is_active = false;
            binding.is_deactivated = true;
        }
    }
}

void ShortcutContext::imgui_menu_item(ShortcutID id, const std::string &name, bool selected)
{
    auto &binding = _key_shortcuts[id];
    if (ImGui::MenuItem(name.c_str(), binding.shortcut_string.c_str(), selected))
        binding.is_activated = true;
}

std::string ShortcutContext::generate_shortcut_string(ModKeys mods, ImGuiKey key)
{
    std::stringstream str;

    if (HAS_BIT((int)mods, (int)ModKeys::CTRL))
        str << "Ctrl+";

    if (HAS_BIT((int)mods, (int)ModKeys::SHIFT))
        str << "Shift+";

    if (HAS_BIT((int)mods, (int)ModKeys::ALT))
        str << "Alt+";

    str << ImGui::GetKeyName(key);
    return str.str();
}