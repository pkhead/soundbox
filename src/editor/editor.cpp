#include <tomlcpp/tomlcpp.hpp>
#include <stb_image.h>
#include <fstream>
#include <nfd.h>

#include "editor.h"
#include "../audio.h"
#include "theme.h"
#include "../ui/ui.h"


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

    add_action("goto_cursor", USERMOD_CTRL, ImGuiKey_H, true);
    add_action("goto_start", USERMOD_CTRL, ImGuiKey_F, true);

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

SongEditor::SongEditor(Song* _song, audiomod::ModuleContext& _modctx, WindowManager& _win_mgr) :
    song(_song), modctx(_modctx), plugin_manager(_win_mgr)
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

    // TODO: customizable default save dir
#ifdef _WIN32
    default_save_dir = std::string(std::getenv("USERPROFILE")) + "\\";
#else
    default_save_dir = std::string(std::getenv("HOME")) + "/";
#endif

    ui_actions.set_callback("song_new", [this]() {
        ui::prompt_unsaved_work([&]() {
            last_file_path.clear();
            last_file_name.clear();
            
            file_mutex.lock();
            //modctx.reset();

            Song* old_song = song;
            song = new Song(4, 8, 8, modctx);
            reset();
            ui::ui_init(*this);
            delete old_song;

            file_mutex.unlock();
        });
    });

    // song save as
    ui_actions.set_callback("song_save_as", [this]() {
        save_song_as();
    });

    // song save
    ui_actions.set_callback("song_save", [this]() {
        save_song();
    });

    // song open
    ui_actions.set_callback("song_open", [this]() {
        nfdchar_t* out_path;
        nfdresult_t result = NFD_OpenDialog("box", last_file_path.empty() ? default_save_dir.c_str() : last_file_path.c_str(), &out_path);

        if (result == NFD_OKAY) {
            std::ifstream file;
            file.open(out_path, std::ios::in | std::ios::binary);

            if (file.is_open()) {
                file_mutex.lock();

                std::string error_msg = "unknown error";
                Song* new_song = Song::from_file(file, modctx, plugin_manager, &error_msg);
                file.close();

                if (new_song != nullptr) {
                    //audio_dest.reset();

                    Song* old_song = song;
                    song = new_song;
                    reset();
                    delete old_song;
                    ui::ui_init(*this);

                    last_file_path = out_path;
                    last_file_name = last_file_path.substr(last_file_path.find_last_of("/\\") + 1);
                } else {
                    ui::show_status("Error reading file: %s", error_msg.c_str());
                }

                file_mutex.unlock();
            } else {
                ui::show_status("Could not open %s", out_path);
            }
        } else if (result != NFD_CANCEL) {
            std::cerr << "Error: " << NFD_GetError() << "\n";
        }
    });

    std::string last_tuning_location;
    ui_actions.set_callback("load_tuning", [this, &last_tuning_location]()
    {
        // only 256 tunings can be loaded
        if (song->tunings.size() >= 256)
        {
            ui::show_status("Cannot add more tunings");
            return;
        }

        nfdchar_t* out_path;
        nfdresult_t result = NFD_OpenDialog(
            "tun,scl,kbm",
            last_tuning_location.empty() ? nullptr : last_tuning_location.c_str(),
            &out_path
        );

        if (result == NFD_OKAY) {
            song->mutex.lock();

            const std::string path_str = std::string(out_path);
            std::string error_msg = "unknown error";

            // get file name extension
            std::string file_ext = path_str.substr(path_str.find_last_of(".") + 1);

            // store location
            last_tuning_location = path_str.substr(0, path_str.find_last_of("/\\") + 1);

            // read scl file
            if (file_ext == "scl")
            {
                Tuning* tun;

                if ((tun = song->load_scale_scl(out_path, &error_msg)))
                {
                    // if tuning name was not found, write file name as name of the tuning
                    if (tun->name.empty())
                    {
                        // get file name without extension
                        std::string file_path = path_str.substr(path_str.find_last_of("/\\") + 1);
                        
                        int dot_index;
                        file_path = (dot_index = file_path.find_last_of(".")) > 0 ?
                            file_path.substr(0, dot_index) :
                            file_path.substr(dot_index + 1); // if dot is at the beginning of file, just remove it

                        tun->name = file_path;
                    } 
                }
                else // error reading file
                {
                    ui::show_status("Error: %s", error_msg.c_str());
                }
            }

            // read kbm file
            else if (file_ext == "kbm")
            {
                Tuning* tun = song->tunings[song->selected_tuning];

                if (tun->scl_import != nullptr)
                {
                    if (song->load_kbm(out_path, *tun, &error_msg))
                    {
                        ui::show_status("Successfully applied mapping");
                    }
                    else // if there was an error
                    {
                        ui::show_status("Error: %s", error_msg.c_str());
                    }
                }
                else // kbm import only works if you have selected a scl import
                {
                    ui::show_status("Please select an SCL import in order to apply the keyboard mapping to it.");
                }
            }

            // read tun file
            else if (file_ext == "tun")
            {
                std::fstream file;
                file.open(out_path);

                if (!file.is_open())
                {
                    ui::show_status("Could not open %s", out_path);
                }
                else
                {
                    Tuning* tun;
                    if ((tun = song->load_scale_tun(file, &error_msg)))
                    {
                        // if tuning name was not found, write file name as name of the tuning
                        if (tun->name.empty())
                        {
                            // get file name without extension
                            std::string file_path = path_str.substr(path_str.find_last_of("/\\") + 1);
                            
                            int dot_index;
                            file_path = (dot_index = file_path.find_last_of(".")) > 0 ?
                                file_path.substr(0, dot_index) :
                                file_path.substr(dot_index + 1); // if dot is at the beginning of file, just remove it

                            tun->name = file_path;
                        } 
                    }
                    else
                    {
                        // error reading file
                        ui::show_status("Error: %s", error_msg.c_str());
                    }
                    
                    file.close();
                }
            }

            // unknown file extension
            else {
                ui::show_status("Incompatible file extension .%s", file_ext.c_str());
            }

            song->mutex.unlock();
        } else if (result != NFD_CANCEL) {
            std::cerr << "Error: " << NFD_GetError() << "\n";
        }
    });

    ui_actions.set_callback("export", [&]() {
        nfdchar_t* out_path = nullptr;
        nfdresult_t result = NFD_SaveDialog("wav", nullptr, &out_path);

        if (result == NFD_OKAY) {
            export_config.active = true;
            export_config.file_name = out_path;
            export_config.sample_rate = 48000;
        } else if (result != NFD_CANCEL) {
            std::cerr << "Error: " << NFD_GetError() << "\n";
            return;
        }
    });
    
    reset();

    last_playing = song->is_playing;
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
    active_notes.clear();
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

void SongEditor::hide_module_interface(audiomod::ModuleNodeRc& mod) {
    for (auto it = mod_interfaces.begin(); it != mod_interfaces.end(); it++) {
        if (*it == mod) {
            mod->module().hide_interface();
            mod_interfaces.erase(it);
            break;
        }
    }
}

void SongEditor::toggle_module_interface(audiomod::ModuleNodeRc& mod) {
    // if want to show interface, add module to interfaces list
    if (!mod->module().interface_shown() && mod->module().show_interface()) {
        mod_interfaces.push_back(mod);

    // if want to hide interface, remove module from interfaces list
    } else {
        hide_module_interface(mod);
    }
}

void SongEditor::delete_fx_bus(std::unique_ptr<audiomod::FXBus>& bus_to_delete)
{
    bus_to_delete->interface_open = false;
    song->delete_fx_bus(bus_to_delete);
}

void SongEditor::remove_channel(int channel_index)
{
    auto& channel = song->channels[channel_index];

    // remove interfaces
    for (auto& mod : channel->effects_rack.modules)
    {
        hide_module_interface(mod);
    }

    song->remove_channel(channel_index);
}

void SongEditor::play_note(int channel, int key, float volume, float secs_len)
{
    if (song->is_note_playable(key))
    {
        audiomod::MidiEvent midi_ev;
        midi_ev.time = modctx.time_in_frames();
        (audiomod::NoteEvent {
            audiomod::NoteEventKind::NoteOn,
            key,
            volume
        }).write_midi(&midi_ev.msg);
        song->channels[channel]->synth_mod->module().queue_event(midi_ev);

        active_notes.push_back({
            key, volume, channel, (int)(modctx.sample_rate * secs_len)
        });
    }
}

void SongEditor::save_preferences() const
{
    std::ofstream file("user.toml", std::ios_base::out | std::ios_base::trunc);

    // write selected theme
    file << "[ui]\n";
    file << "theme = " << std::quoted(theme.name()) << "\n";
    file << "follow_playhead = " << (follow_playhead ? "true" : "false") << "\n";
    file << "note_preview = " << (note_preview ? "true" : "false") << "\n";
    file << "show_all_channels = " << (show_all_channels ? "true" : "false") << "\n";

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

        auto follow_playhead_v = ui_table->getBool("follow_playhead");
        if (follow_playhead_v.first) {
            follow_playhead = follow_playhead_v.second;
        }

        auto note_preview_v = ui_table->getBool("note_preview");
        if (note_preview_v.first) {
            note_preview = note_preview_v.second;
        }

        auto show_all_channels_v = ui_table->getBool("show_all_channels");
        if (show_all_channels_v.first) {
            show_all_channels = show_all_channels_v.second;
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

bool SongEditor::save_song_as()
{
    std::string file_name = last_file_path.empty()
        ? default_save_dir + this->song->name + ".box"
        : last_file_path;
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
}

bool SongEditor::save_song()
{
    if (last_file_path.empty()) {
        return save_song_as();
    }

    std::ofstream file;
    file.open(last_file_path, std::ios::out | std::ios::trunc | std::ios::binary);

    if (file.is_open()) {
        song->serialize(file);
        file.close();

        ui::show_status("Successfully saved %s", last_file_path.c_str());
        return true;
    } else {
        ui::show_status("Could not save to %s", last_file_path.c_str());
        return false;
    }
}

void SongEditor::process(AudioDevice& device)
{
    file_mutex.lock();
    song->mutex.lock();

    //modctx.prepare();

    bool song_playing = song->is_playing;
    if (song_playing != last_playing) {
        last_playing = song_playing;
        
        if (song_playing) song->play();
        else song->stop();
    }

    int frames_passed = 0;

    while (device.samples_queued() < device.sample_rate() * 0.05)
    {
        float* buf;
        if (song_playing) song->update((double)modctx.frames_per_buffer / device.sample_rate());
        size_t buf_size = modctx.process(buf);
        device.queue(buf, buf_size);

        frames_passed += modctx.frames_per_buffer;
    }

    // cursor follow playhead (if user enabled this feature)
    if (follow_playhead && song_playing)
        selected_bar = song->bar_position;

    // process active notes (started from note previews)
    for (int i = active_notes.size() - 1; i >= 0; i--)
    {
        active_note_t& active_note = active_notes[i];
        active_note.frames_remaining -= frames_passed;

        // stop note when done
        if (active_note.frames_remaining <= 0)
        {
            // if channel still exists
            if (active_note.channel < song->channels.size())
            {
                audiomod::MidiEvent midi_ev;
                midi_ev.time = modctx.time_in_frames();
                (audiomod::NoteEvent {
                    audiomod::NoteEventKind::NoteOff,
                    active_note.key,
                    active_note.volume
                }).write_midi(&midi_ev.msg);

                song->channels[active_note.channel]->synth_mod->module().queue_event(midi_ev);
            }

            active_notes.erase(active_notes.begin() + i);
        }
    }

    file_mutex.unlock();
    song->mutex.unlock();

    if (song_export)
        song_export->process();
}

void SongEditor::begin_export()
{
    if (song_export) return;
    song_export = std::make_unique<SongExport>(*this, export_config.file_name, export_config.sample_rate);

    // if there was an error in song export
    if (!song_export->error().empty())
    {
        ui::show_status(song_export->error());
        song_export = nullptr;
    }
}

void SongEditor::stop_export()
{
    if (!song_export) return;
    song_export = nullptr;
}