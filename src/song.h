#pragma once

#include <algorithm>
#include <cstdint>
#include <sys/types.h>
#include <unordered_map>
#include <vector>
#include <ostream>
#include <istream>
#include <string>
#include <mutex>

#include <TUN_Scale.h>
#include <Tunings.h>
#include "audio.h"
#include <imgui.h>
#include "modules/volume.h"

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

class Song;

class Channel {
private:
    std::vector<audiomod::FXBus*>& fx_mixer;

public:
    Channel(const Channel&) = delete; // prevent copy
    Channel(int song_length, int max_patterns, Song* song);
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

struct TuningSclImport
{
    Tunings::Scale scl;
    Tunings::KeyboardMapping kbm;
};

struct Tuning
{
    std::string name;
    std::string desc;
    bool is_12edo = false;

    std::vector<float> key_freqs;
    
    struct KeyInfoStruct
    {
        bool is_octave;
        bool is_fifth;
        int octave_number;
        double ratio_float;
        int ratio_num;
        int ratio_den;
        ImU32 key_color;
    };

    std::vector<KeyInfoStruct> key_info;

    // SCL import data
    // if this is nullptr, then it is assumed this is a .tun import
    TuningSclImport* scl_import = nullptr;

    // perform analysis for pattern editor display
    void analyze();
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

    std::string project_notes;

    std::vector<Channel*> channels;

    std::vector<audiomod::FXBus*> fx_mixer;

    int beats_per_bar = 8;
    int bar_position = 0;
    double position = 0.0;
    bool is_playing = false;
    bool do_loop = true;
    float tempo = 120;

    // there can be multiple tunings
    // and song may be able to switch between them while playing
    std::vector<Tuning*> tunings;
    int selected_tuning = 0;
    
    int length() const;
    void set_length(int len);

    int max_patterns() const;
    void set_max_patterns(int num_patterns);

    int new_pattern(int channel_index);

    void insert_bar(int bar_position);
    void remove_bar(int bar_position);

    Channel* insert_channel(int channel_index);
    void remove_channel(int channel_index);

    void delete_fx_bus(audiomod::FXBus* bus_to_delete);

    bool get_key_frequency(int key, float* freq) const;

    void play();
    void stop();
    void update(double elasped);

    /**
    * Load scale data in the AnaMark tuning file format
    * @param input the input stream
    * @param error the pointer to the string which may hold the error message
    * @returns the newly created tuning, or nullptr if there was an error 
    **/
    Tuning* load_scale_tun(std::istream& input, std::string* error);

    /**
    * Load scale data in the Scala file format
    * @param input path to the input file
    * @param error the pointer to the string which may hold the error message
    * @returns the newly created tuning, or nullptr if there was an error
    */
    Tuning* load_scale_scl(const char* file_path, std::string* error);

    /**
    * Load keyboard mappings for a Scala import
    * @param input path to the input file
    * @param tuning a reference to the tuning to modify
    * @param error pointer to the string which may hold the error message
    * @returns true if successful, false if there was an error
    */
    bool load_kbm(const char* file_path, Tuning& tuning, std::string* error);

    void serialize(std::ostream& out) const;
    static Song* from_file(std::istream& input, audiomod::ModuleOutputTarget& audio_out, std::string *error_msg);
};