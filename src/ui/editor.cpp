#include <tomlcpp/tomlcpp.hpp>
#include <fstream>
#include "editor.h"
#include "../audio.h"
#include "theme.h"

//////////////////
// USER ACTIONS //
//////////////////

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
    add_action("song_play_pause", 0, ImGuiKey_Space, true);
    add_action("song_prev_bar", 0, ImGuiKey_LeftBracket, true);
    add_action("song_next_bar", 0, ImGuiKey_RightBracket, true);
    add_action("export", 0, ImGuiKey_None);
    add_action("quit", 0, ImGuiKey_None);

    add_action("undo", USERMOD_CTRL, ImGuiKey_Z, true);
#ifdef _WIN32
    add_action("redo", USERMOD_CTRL, ImGuiKey_Y, true);
#else
    add_action("redo", USERMOD_CTRL | USERMOD_SHIFT, ImGuiKey_Z, true);
#endif
    add_action("copy", USERMOD_CTRL, ImGuiKey_C);
    add_action("paste", USERMOD_CTRL, ImGuiKey_V);

    add_action("new_channel", USERMOD_CTRL, ImGuiKey_Enter, true);
    add_action("remove_channel", USERMOD_CTRL, ImGuiKey_Backspace, true);
    add_action("insert_bar", 0, ImGuiKey_Enter, true);
    add_action("insert_bar_before", USERMOD_SHIFT, ImGuiKey_Enter, true);
    add_action("remove_bar", 0, ImGuiKey_Backspace);

    add_action("new_pattern", USERMOD_CTRL, ImGuiKey_P, false);

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

//////////////////////////
// SongEditor singleton //
//////////////////////////

SongEditor::SongEditor(Song* song, audiomod::DestinationModule& audio_dest) :
    song(song), audio_dest(audio_dest)
{
    const char* theme_name = "Soundbox Dark";

    try
    {
        theme = Theme(theme_name);
    } catch(...) {
        std::cerr << "error: could not find theme \"" << theme_name << "\"\n";
    }

    theme.set_imgui_colors();
    plugin_manager.scan_plugins();
    
    reset();
}

SongEditor::~SongEditor()
{
    delete song;

    for (auto it : ui_values)
        free(it.second);
}

void SongEditor::reset()
{
    undo_stack.clear();
    redo_stack.clear();
    ui_values.clear();
    mod_interfaces.clear();
    fx_interfaces.clear();
    selected_channel = 0;
    selected_bar = 0;
}

bool SongEditor::undo()
{
    if (undo_stack.is_empty()) return false;

    change::Action* action = undo_stack.pop();
    action->undo(*this);
    redo_stack.push(action);
    return true;
}

bool SongEditor::redo()
{
    if (redo_stack.is_empty()) return false;

    change::Action* action = redo_stack.pop();
    action->redo(*this);
    undo_stack.push(action);
    return true;
}

void SongEditor::hide_module_interface(audiomod::ModuleBase* mod) {
    for (auto it = mod_interfaces.begin(); it != mod_interfaces.end(); it++) {
        if (*it == mod) {
            mod_interfaces.erase(it);
            break;
        }
    }
}

void SongEditor::toggle_module_interface(audiomod::ModuleBase* mod) {
    mod->show_interface = !mod->show_interface;
    
    // if want to show interface, add module to interfaces list
    if (mod->show_interface) {
        mod_interfaces.push_back(mod);

    // if want to hide interface, remove module from interfaces list
    } else {
        hide_module_interface(mod);
    }
}

void SongEditor::delete_fx_bus(audiomod::FXBus* bus_to_delete)
{
    // remove interface
    bus_to_delete->interface_open = false;
    for (auto it = fx_interfaces.begin(); it != fx_interfaces.end(); it++)
        if (*it == bus_to_delete) {
            fx_interfaces.erase(it);
            break;
        }

    song->delete_fx_bus(bus_to_delete);
}

void SongEditor::remove_channel(int channel_index)
{
    Channel* channel = song->channels[channel_index];

    // remove interfaces
    for (audiomod::ModuleBase* mod : channel->effects_rack.modules)
    {
        hide_module_interface(mod);
    }

    song->remove_channel(channel_index);
}

void SongEditor::save_preferences() const
{
    std::ofstream file("user.toml", std::ios_base::out | std::ios_base::trunc);

    // write selected theme
    file << "[ui]\n";
    file << "theme = " << std::quoted(theme.name()) << "\n";

    // write plugin paths
    file << "\n[plugins]\n";

    // ladspa
    file << "ladspa = [";

    int i = 0;
    const std::vector<std::string>& system_paths = plugin_manager.get_standard_plugin_paths(plugins::PluginType::Ladspa);
    for (const std::string& path : plugin_manager.ladspa_paths)
    {
        bool is_system_path = false;

        // exclude system paths
        for (const std::string& other : system_paths) {
            if (path == other) {
                is_system_path = true;
                break;
            }
        }

        if (is_system_path) continue;

        if (i == 0)
            file << "\n";

        file << "\t" << std::quoted(path) << "\n";
        i++;
    }

    file << "]\n";

    file.close();
}

void SongEditor::load_preferences()
{
    auto data = toml::parseFile("user.toml");
    if (!data.table) {
        std::cerr << "error parsing preferences: " << data.errmsg << "\n";
        return;
    }

    // get ui preferences
    auto ui_table = data.table->getTable("ui");
    if (ui_table)
    {
        auto theme_pref = ui_table->getString("theme");
        if (theme_pref.first) {
            theme.load(theme_pref.second);
            theme.set_imgui_colors();
        }
    }

    // get plugin paths
    auto plugins = data.table->getTable("plugins");
    if (plugins)
    {
        // read ladspa paths
        auto ladspa_paths = plugins->getArray("ladspa");
        if (ladspa_paths)
        {
            for (int i = 0; ; i++) {
                auto p = ladspa_paths->getString(i);
                if (!p.first) break;
                plugin_manager.add_path(plugins::PluginType::Ladspa, p.second);
            }

        }
    }
}