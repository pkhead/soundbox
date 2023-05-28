#pragma once
#include <vector>
#include <ostream>
#include <istream>
#include <string>
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
    audiomod::ModuleOutputTarget& audio_out;

public:
    Channel(const Channel&) = delete; // prevent copy
    Channel(int song_length, int max_patterns, audiomod::ModuleOutputTarget& audio_out);
    ~Channel();

    audiomod::VolumeModule vol_mod;
    audiomod::ModuleBase* synth_mod;
    audiomod::EffectsRack effects_rack;

    int selected_effect = 0;

    char name[16];
    std::vector<int> sequence;
    std::vector<Pattern*> patterns;
    
    void set_instrument(audiomod::ModuleBase* new_instrument);
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

    static constexpr size_t name_capcity = 128;
    char name[name_capcity];

    std::vector<Channel*> channels;
    int selected_channel = 0;
    int selected_bar = 0;

    std::vector<Note> note_clipboard;

    int beats_per_bar = 8;
    int bar_position = 0;
    double position = 0.0;
    bool is_playing = false;
    bool do_loop = true;

    float editor_quantization = 0.25f;
    float tempo = 120;

    struct ModInterfaceStruct {
        Channel* channel;
        audiomod::ModuleBase* module;
    };

    std::vector<ModInterfaceStruct> mod_interfaces;

    // An effect index of -1 means the instrument module
    void toggle_module_interface(int channel_index, int effect_index);
    void hide_module_interface(audiomod::ModuleBase* mod);
    
    int length() const;
    void set_length(int len);

    int max_patterns() const;
    void set_max_patterns(int num_patterns);

    int new_pattern(int channel_id);

    float get_key_frequency(int key) const;

    void play();
    void stop();
    void update(double elasped);

    void serialize(std::ostream& out) const;
    static Song* from_file(std::istream& input, audiomod::ModuleOutputTarget& audio_out, std::string *error_msg);
};