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
#include "plugins.h"
#include <imgui.h>
#include "modules/modules.h"
#include "worker.h"

struct Note {
    float time;
    int key;
    float length;

    // Notes are not dynamically allocated so
    // we can't use their pointers for identification
    // this is why we make a unique id for all new notes
    unsigned long id;

    Note();
    Note(float time, int key, float length);

    inline bool operator==(const Note& other) const noexcept {
        return id == other.id;
    }

    inline bool operator!=(const Note& other) const noexcept {
        return !(*this == other);
    }
};

struct Pattern {
    Pattern();

    std::vector<Note> notes;
    Note& add_note(float time, int key, float length);
    inline bool is_empty() const; 
};

class Song;

class Channel {
private:
    Song& song;

public:
    Channel(const Channel&) = delete; // prevent copy
    Channel(int song_length, int max_patterns, Song& song);
    ~Channel();

    audiomod::ModuleNodeRc vol_mod;
    audiomod::ModuleNodeRc synth_mod;
    audiomod::EffectsRack effects_rack;

    int fx_target_idx = 0;
    bool solo = false;

    char name[16];
    std::vector<int> sequence;
    std::vector<std::unique_ptr<Pattern>> patterns;
    
    int first_empty_pattern() const;
    void set_instrument(audiomod::ModuleNodeRc new_instrument);
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

    struct NoteData {
        int channel_i;
        Note note;
    };

    std::vector<NoteData> prev_notes;
    std::vector<NoteData> cur_notes;
    audiomod::ModuleContext& modctx;

    // this variable is solely for debug purpose
    int notes_playing = 0;

public:
    Song(const Song&) = delete; // disable copy
    Song(int num_channels, int length, int max_patterns, audiomod::ModuleContext& modctx);
    
    std::mutex mutex;

    // scheduler for work to be done by modules
    WorkScheduler work_scheduler;

    static constexpr size_t name_capcity = 128;
    char name[name_capcity];

    std::string project_notes;

    std::vector<std::unique_ptr<audiomod::FXBus>> fx_mixer;
    std::vector<std::unique_ptr<Channel>> channels;

    int beats_per_bar = 8;
    int bar_position = 0;
    double position = 0.0;
    std::atomic<bool> is_playing = false;
    bool do_loop = true;
    float tempo = 120;

    // there can be multiple tunings
    // and song may be able to switch between them while playing
    std::vector<Tuning*> tunings;
    int selected_tuning = 0;

    inline audiomod::ModuleContext& mod_ctx() const {
        return modctx;
    };
    
    int length() const;
    int max_patterns() const;
    void set_max_patterns(int num_patterns);

    int new_pattern(int channel_index);

    void insert_bar(int bar_position);
    void remove_bar(int bar_position);
    std::vector<int> get_bar_patterns(int bar_position);
    void set_bar_patterns(int bar_position, int* array, size_t size);
    void set_bar_patterns(int bar_position, std::vector<int> patterns);

    std::unique_ptr<Channel>& insert_channel(int channel_index);
    void remove_channel(int channel_index);

    void delete_fx_bus(std::unique_ptr<audiomod::FXBus>& bus_to_delete);

    bool is_note_playable(int key) const;
    bool get_key_frequency(int key, float* freq) const;

    // only call these functions from the processing thread
    // other threads set is_playing
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
    static std::unique_ptr<Song> from_file(
        std::istream& input,
        audiomod::ModuleContext& audio_dest,
        plugins::PluginManager& plugin_manager,
        std::string *error_msg
    );
};