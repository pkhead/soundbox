#pragma once
#include "../audio.h"
#include <imgui.h>
#include <mutex>
#include "theme.h"
#include "change_history.h"
#include "../plugins.h"
#include "../song.h"
#include "../audiofile.h"

constexpr uint8_t USERMOD_SHIFT = 1;
constexpr uint8_t USERMOD_CTRL = 2;
constexpr uint8_t USERMOD_ALT = 4;

struct UserAction {
    std::string name;
    uint8_t modifiers;
    ImGuiKey key;
    std::function<void()> callback;
    std::string combo;
    bool do_repeat;

    UserAction(const std::string& name, uint8_t mod, ImGuiKey key, bool repeat = false);
    void set_keybind(uint8_t mod, ImGuiKey key);
};

struct UserActionList {
    std::vector<UserAction> actions;

    UserActionList();
    void add_action(const std::string& action_name, uint8_t mod, ImGuiKey key, bool repeat = false);
    void fire(const std::string& action_name) const;
    void set_keybind(const std::string& action_name, uint8_t mod, ImGuiKey key);
    void set_callback(const std::string& action_name, std::function<void()> callback);
    const char* combo_str(const std::string& action_name) const;

    /*
    UserAction song_save;
    UserAction song_save_as;
    UserAction song_open;
    
    UserAction song_play_pause;
    UserAction song_prev_bar;
    UserAction song_next_bar;
    UserAction quit;
    */
};

class SongExport;

class SongEditor {
private:
    // this mutex is locked by the audio thread while audio is being processed
    // and is locked by the main thread when a new song is being loaded
    std::mutex file_mutex;

    std::string last_file_path;
    std::string last_file_name;
    std::string default_save_dir;

    std::unique_ptr<SongExport> song_export;
    bool last_playing;
public:
    SongEditor(Song* song, audiomod::DestinationModule& audio_dest, WindowManager& winmgr);
    ~SongEditor();
    Song* song;
    Theme theme;
    UserActionList ui_actions;
    audiomod::DestinationModule& audio_dest;
    plugins::PluginManager plugin_manager;

    // exporting
    struct ExportConfigData {
        bool active = false;
        char* file_name = nullptr;
        int sample_rate = 0;
    } export_config;
    
    void reset();
    void save_preferences() const;
    void load_preferences();

    bool save_song();
    bool save_song_as();

    void process(AudioDevice& device);


    int selected_channel = 0;
    int selected_bar = 0;

    // TODO: use system clipboard
    std::vector<Note> note_clipboard;

    float quantization = 0.25f;
    
    bool show_tuning_window = false;
    bool show_themes_window = false;
    bool show_plugin_list = false;
    bool show_dir_window = false;

    std::vector<audiomod::ModuleBase*> mod_interfaces;
    std::vector<audiomod::FXBus*> fx_interfaces;

    void begin_export();
    void stop_export();
    inline const std::unique_ptr<SongExport>& current_export() {
        return song_export;
    };

    // the previous values of monitored UI inputs
    std::unordered_map<ImGuiID, void*> ui_values;

    // returns true if there was something to undo
    bool undo();

    // returns true if there was something to redo
    bool redo();

    // push a change to the undo stack
    inline void push_change(change::Action* action) {
        undo_stack.push(action);
        redo_stack.clear();    
    };

    change::Stack undo_stack;
    change::Stack redo_stack;

    void remove_channel(int channel_index);
    void delete_fx_bus(audiomod::FXBus* bus_to_delete);
    void toggle_module_interface(audiomod::ModuleBase* mod);
    void hide_module_interface(audiomod::ModuleBase* mod);
};

class SongExport
{
private:
    std::string _error;

    bool is_done;
    std::unique_ptr<Song> song; // a copy of the current song for the export process
    SongEditor& editor;
    audiomod::DestinationModule destination;
    size_t total_frames;
    std::ofstream out_file;
    std::unique_ptr<audiofile::WavWriter> writer;
    int _step;

public:
    SongExport(SongEditor& editor, const char* file_name, int sample_rate);

    float get_progress() const;
    inline std::string error() const { return _error; };
    
    inline bool finished() const { return is_done; };
    void cancel();
    void process();
};