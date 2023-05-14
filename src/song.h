#pragma once
#include <vector>
#include <string>
#include "audio.h"

struct Note {
    float time;
    int key;
    float length;
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

    audiomod::WaveformSynth synth_mod;

    char name[32];
    float volume;
    float panning;
    std::vector<int> sequence;
    std::vector<Pattern*> patterns;

    // instrument
    // volumeModule

    // public async loadInstrument
};

class Song {
private:
    int _length;
    int _max_patterns;
    audiomod::ModuleOutputTarget& audio_out;

public:
    Song(const Song&) = delete; // disable copy
    Song(int num_channels, int length, int max_patterns, audiomod::ModuleOutputTarget& audio_out);
    ~Song();

    std::vector<Channel*> channels;
    int selected_channel = 0;
    int selected_bar = 0;

    int position = 0;
    float tempo = 120;
    
    int length() const;
    void set_length(int len);

    int max_patterns() const;
    void set_max_patterns(int num_patterns);

    int new_pattern(int channel_id);

    float get_key_frequency(int key) const;
};