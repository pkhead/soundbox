#pragma once
#include <vector>
#include <ostream>
#include <istream>
#include <string>
#include <mutex>
#include "audio.h"
#include "modules/modules.h"

struct Note {
    float time;
    int key;
    float length;

    bool operator==(const Note& other) {
        return time == other.time && key == other.key && length == other.length;
    }
};

struct Pattern {
    Pattern(const Pattern&) = delete; // prevent copying
    Pattern();

    std::vector<Note> notes;
    Note& add_note(float time, int key, float length);
    inline bool is_empty() const; 
};

class Channel {
private:
    std::vector<audiomod::FXBus*>& fx_mixer;

public:
    Channel(const Channel&) = delete; // prevent copy
    Channel(int song_length, int max_patterns, std::vector<audiomod::FXBus*>& fx_mixer);
    ~Channel();

    audiomod::VolumeModule vol_mod;
    audiomod::ModuleBase* synth_mod;
    audiomod::EffectsRack effects_rack;

    int fx_target_idx = 0;
    bool solo = false;

    char name[16];
    std::vector<int> sequence;
    std::vector<Pattern*> patterns;
    
    void set_instrument(audiomod::ModuleBase* new_instrument);
    void set_fx_target(int fx_index);
};

struct Tuning
{
    std::string name;
    std::string desc;
    std::vector<float> pitches;
};

class Song {
private:
    int _length;
    int _max_patterns;
    audiomod::ModuleOutputTarget& audio_out;

    struct NoteData {
        int channel_i;
        Note note;
    };

    std::vector<NoteData> prev_notes;
    std::vector<NoteData> cur_notes;

public:
    Song(const Song&) = delete; // disable copy
    Song(int num_channels, int length, int max_patterns, audiomod::ModuleOutputTarget& audio_out);
    ~Song();

    std::mutex mutex;

    static constexpr size_t name_capcity = 128;
    char name[name_capcity];

    std::vector<Channel*> channels;
    int selected_channel = 0;
    int selected_bar = 0;

    std::vector<audiomod::FXBus*> fx_mixer;

    // TODO: use system clipboard
    std::vector<Note> note_clipboard;

    int beats_per_bar = 8;
    int bar_position = 0;
    double position = 0.0;
    bool is_playing = false;
    bool do_loop = true;

    float editor_quantization = 0.25f;
    float tempo = 120;

    // there can be multiple tunings
    // and song may be able to switch between them while playing
    std::vector<Tuning*> tunings;
    int selected_tuning = 0;
    bool show_tuning_window = false;

    std::vector<audiomod::ModuleBase*> mod_interfaces;
    std::vector<audiomod::FXBus*> fx_interfaces;

    // An effect index of -1 means the instrument module
    void toggle_module_interface(audiomod::ModuleBase* mod);
    void hide_module_interface(audiomod::ModuleBase* mod);
    
    int length() const;
    void set_length(int len);

    int max_patterns() const;
    void set_max_patterns(int num_patterns);

    int new_pattern(int channel_index);

    void insert_bar(int bar_position);
    void remove_bar(int bar_position);

    Channel* insert_channel(int channel_index);
    void remove_channel(int channel_index);

    float get_key_frequency(int key) const;

    void play();
    void stop();
    void update(double elasped);

    /**
    * Load scale data in the Scala format
    * @param error the pointer to the string which may hold the error message
    * @returns the newly created Scale, or nullptr if there was an error 
    **/
    Tuning* load_scale_scl(std::istream& input, std::string* error);

    void serialize(std::ostream& out) const;
    static Song* from_file(std::istream& input, audiomod::ModuleOutputTarget& audio_out, std::string *error_msg);
};