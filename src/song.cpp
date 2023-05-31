#include <assert.h>
#include <cstdint>
#include <cstdio>
#include <string.h>
#include <math.h>
#include "audio.h"
#include "song.h"
#include "sys.h"

Pattern::Pattern() { };

Note& Pattern::add_note(float time, int key, float length) {
    notes.push_back({ time, key, length });
    return *(notes.end() - 1);
}

inline bool Pattern::is_empty() const {
    return notes.empty();
}

/*************************
*        CHANNEL         *
*************************/
Channel::Channel(int song_length, int max_patterns, std::vector<audiomod::FXBus*>& fx_mixer) : fx_mixer(fx_mixer) {
    strcpy(name, "Channel");
    
    for (int i = 0; i < song_length; i++) {
        sequence.push_back(0);
    }

    for (int i = 0; i < max_patterns; i++) {
        patterns.push_back(new Pattern());
    }

    synth_mod = new audiomod::WaveformSynth();
    effects_rack.connect_input(synth_mod);
    effects_rack.connect_output(&vol_mod);
    fx_mixer[0]->connect_input(&vol_mod);
}

Channel::~Channel() {
    for (Pattern* pattern : patterns) {
        delete pattern;
    }

    effects_rack.disconnect_all_inputs();
    effects_rack.disconnect_output();

    fx_mixer[0]->disconnect_input(&vol_mod);
    delete synth_mod;
}

void Channel::set_instrument(audiomod::ModuleBase* new_instrument) {
    effects_rack.disconnect_input(synth_mod);
    if (synth_mod != nullptr) delete synth_mod;
    
    synth_mod = new_instrument;
    effects_rack.connect_input(synth_mod);
}


/*************************
*         SONG           *
*************************/

Song::Song(int num_channels, int length, int max_patterns, audiomod::ModuleOutputTarget& audio_out) : audio_out(audio_out), _length(length), _max_patterns(max_patterns) {
    strcpy(name, "Untitled");

    audiomod::FXBus* master_bus = new audiomod::FXBus();
    strcpy(master_bus->name, "Master");
    master_bus->connect_output(&audio_out);
    fx_mixer.push_back(master_bus);

    for (int i = 0; i < 3; i++)
    {
        audiomod::FXBus* bus = new audiomod::FXBus();
        snprintf(bus->name, bus->name_capacity, "Bus %i", i);
        bus->connect_output(&audio_out);
        fx_mixer.push_back(bus);
    }
    
    for (int ch_i = 0; ch_i < num_channels; ch_i++) {
        Channel* ch = new Channel(_length, _max_patterns, fx_mixer);
        snprintf(ch->name, 32, "Channel %i", ch_i + 1);
        channels.push_back(ch);
    }
}

Song::~Song() {
    for (Channel* channel : channels) {
        delete channel;
    }

    for (audiomod::FXBus* bus : fx_mixer)
    {
        delete bus;
    }
}

// length field
int Song::length() const { return _length; }
void Song::set_length(int len) {
    _length = len;
    // TODO change channel sequence arrays
}

// max_pattern field
int Song::max_patterns() const { return _max_patterns; }
void Song::set_max_patterns(int num_patterns) {
    if (num_patterns > _max_patterns) {
        // add patterns
        for (Channel* ch : channels) {
            while (ch->patterns.size() < num_patterns) ch->patterns.push_back(new Pattern());
        }
    } else if (num_patterns < _max_patterns) {
        // remove patterns
        for (Channel* ch : channels) {
            for (auto it = ch->patterns.begin() + num_patterns; it != ch->patterns.end(); it++) {
                delete *it;
            }
            ch->patterns.erase(ch->patterns.begin() + num_patterns, ch->patterns.end());

            assert(ch->patterns.size() != num_patterns);
        }
    }

    _max_patterns = num_patterns;
}

int Song::new_pattern(int channel_id) {
    if (channel_id < 0 || channel_id >= channels.size()) return -1;
    Channel* channel = channels[channel_id];

    // find first unused pattern slot in channel
    for (int i = 0; i < channel->patterns.size(); i++) {
        if (channel->patterns[i]->is_empty()) return i + 1;
    }

    // no unused patterns found, create a new one
    set_max_patterns(_max_patterns + 1);
    return _max_patterns;
}

void Song::insert_bar(int position)
{
    mutex.lock();

    _length++;

    for (Channel* channel : channels) {
        channel->sequence.insert(channel->sequence.begin() + position, 0);
    }

    mutex.unlock();
}

void Song::remove_bar(int position)
{
    mutex.lock();

    _length--;

    for (Channel* channel : channels) {
        channel->sequence.erase(channel->sequence.begin() + position);
    }

    if (bar_position >= _length)
    {
        bar_position = _length - 1;
        position = bar_position * beats_per_bar;
    }

    mutex.unlock();
}

Channel* Song::insert_channel(int channel_id)
{
    mutex.lock();

    Channel* new_channel = new Channel(_length, _max_patterns, fx_mixer);
    snprintf(new_channel->name, 16, "Channel %i", (int) channels.size() + 1);
    channels.insert(channels.begin() + channel_id, new_channel);

    mutex.unlock();

    return new_channel;
}

void Song::remove_channel(int channel_index)
{
    mutex.lock();

    Channel* ch = channels[channel_index];
    channels.erase(channels.begin() + channel_index);
    delete ch;

    mutex.unlock();
}

float Song::get_key_frequency(int key) const {
    return powf(2.0f, (key - 57) / 12.0f) * 440.0;
}

void Song::play() {
    is_playing = true;
    position = bar_position * beats_per_bar;

    cur_notes.clear();
    prev_notes.clear();
}

void Song::stop() {
    is_playing = false;
    position = bar_position * beats_per_bar;

    for (NoteData note_data : cur_notes) {
        channels[note_data.channel_i]->synth_mod->event({
            audiomod::NoteEventKind::NoteOff,
            note_data.note.key
        });
    }

    prev_notes.clear();
    cur_notes.clear();
}

void Song::update(double elapsed) {
    if (is_playing) {
        // first check if any channels are solo'd
        bool solo_mode = false;

        for (Channel* channel : channels)
        {
            if (channel->solo)
            {
                solo_mode = true;
                break;
            }
        }

        // if a channel is solo'd, then mute all but the channels that are solo'd
        if (solo_mode)
        {
            for (Channel* channel : channels)
            {
                channel->vol_mod.mute_override = !channel->solo;
            }
        }
        
        // no channels are solo'd
        else {
            for (Channel* channel : channels)
            {
                channel->vol_mod.mute_override = false;
            }
        }

        // the method for soloing is probably like, the least efficient method possible
        // But it doesn't matter until the user creates 10,000 channels

        // get notes at playhead
        float pos_in_bar = fmod(position, (double)beats_per_bar);
        cur_notes.clear();

        int channel_i = 0;
        for (Channel* channel : channels) {
            int pattern_index = channel->sequence[bar_position] - 1;
            if (pattern_index >= 0) {
                Pattern* pattern = channel->patterns[pattern_index];

                for (Note& note : pattern->notes) {
                    if (pos_in_bar > note.time && pos_in_bar < note.time + note.length)    
                        cur_notes.push_back({
                            channel_i,
                            note
                        });
                }
            }

            channel_i++;
        }

        // if there are notes in cur_notes that are not in prev_notes
        // they are new notes
        for (NoteData& new_note : cur_notes) {
            bool is_new = true;

            for (NoteData& old_note : prev_notes) {
                if (new_note.note == old_note.note) {
                    is_new = false;
                    break;
                }
            }

            if (is_new) {
                channels[new_note.channel_i]->synth_mod->event({
                    audiomod::NoteEventKind::NoteOn,
                    {
                        new_note.note.key,
                        get_key_frequency(new_note.note.key),
                        0.8f
                    }
                });
            }
        }

        // if there are notes in prev_notes that are not in cur_notes
        // they are notes that just ended
        for (NoteData& old_note : prev_notes) {
            bool is_old = true;

            for (NoteData& new_note : cur_notes) {
                if (new_note.note == old_note.note) {
                    is_old = false;
                    break;
                }
            }

            if (is_old) {
                channels[old_note.channel_i]->synth_mod->event({
                    audiomod::NoteEventKind::NoteOff,
                    old_note.note.key
                });
            }
        }

        prev_notes = cur_notes;

        position += elapsed * (tempo / 60.0);

        // if reached past the end of the song
        if (position >= _length * beats_per_bar) {
            if (do_loop) // loop back to the beginning of the song
                position -= _length * beats_per_bar;
            else {
                // stop the song and set cursor to the beginning
                bar_position = 0;
                position = 0;
                stop();
                return;
            }
        }

        bar_position = position / beats_per_bar;
    }
}

void Song::hide_module_interface(audiomod::ModuleBase* mod) {
    for (auto it = mod_interfaces.begin(); it != mod_interfaces.end(); it++) {
        if ((*it).module == mod) {
            mod_interfaces.erase(it);
            break;
        }
    }
}

void Song::toggle_module_interface(int channel_index, int effect_index) {
    Channel* cur_channel = channels[channel_index];
    audiomod::ModuleBase* mod;

    if (effect_index < 0)
        mod = cur_channel->synth_mod;
    else {
        if (effect_index >= cur_channel->effects_rack.modules.size()) return;
        mod = cur_channel->effects_rack.modules[effect_index];
    }

    mod->show_interface = !mod->show_interface;
    
    // if want to show interface, add module to interfaces list
    if (mod->show_interface) {
        mod_interfaces.push_back({
            cur_channel,
            mod
        });

    // if want to hide interface, remove module from interfaces list
    } else {
        hide_module_interface(mod);
    }
}

/*
Song data structure:

struct note {
    float time;
    int16_t key;
    float length;
}

struct pattern {
    uint32_t num_notes;
    struct note notes[num_notes];
}

struct channel {
    uint8_t name_size;
    char name[name_size];

    float volume, panning;
    uint8_t inst_id_size;
    char inst_id[inst_id_size];
    uint64_t inst_state_size;
    uint8_t inst_state[inst_state_size];

    uint32_t sequence[song::length];
    struct pattern patterns[song::max_patterns];
}

struct song {
    uint8_t name_size;
    char name[name_size];

    uint32_t length;
    uint32_t num_channels;
    uint32_t max_patterns;
    uint32_t beats_per_bar;
    float tempo;

    struct channel channels[num_channels];
}

struct file {
    char magic[4] = "SnBx";
    uint8_t version_major;
    uint8_t version_minor;
    uint8_t version_revision;

    struct song song_data;
}
*/
void Song::serialize(std::ostream& out) const {
    // magic number
    out << "SnBx";

    // version number (major, minor, revision)
    push_bytes(out, (uint8_t)0);
    push_bytes(out, (uint8_t)0);
    push_bytes(out, (uint8_t)2);

    // write song data
    push_bytes(out, (uint8_t) strlen(name));
    out << name;

    push_bytes(out, (uint32_t) length());
    push_bytes(out, (uint32_t) channels.size());
    push_bytes(out, (uint32_t) max_patterns());
    push_bytes(out, (uint32_t) beats_per_bar);
    push_bytes(out, (float) tempo);

    // write the channel data
    for (Channel* channel : channels) {
        push_bytes(out, (uint8_t) strlen(channel->name));
        out << channel->name;

        push_bytes(out, (float) channel->vol_mod.volume);
        push_bytes(out, (float) channel->vol_mod.panning);

        // write channel flags (mute, solo)
        uint8_t channel_flags = 0;
        if (channel->vol_mod.mute)  channel_flags |= 1;
        if (channel->solo)          channel_flags |= 2;

        push_bytes(out, (uint8_t) channel_flags);

        // store instrument type
        push_bytes(out, (uint8_t) strlen(channel->synth_mod->id));
        out << channel->synth_mod->id;

        // store instrument state
        uint8_t* inst_state = nullptr;
        uint64_t inst_state_size = channel->synth_mod->save_state((void**)&inst_state);

        push_bytes(out, inst_state_size);
        if (inst_state != nullptr) {
            out.write((char*)inst_state, inst_state_size);
            delete inst_state;
        }

        // write channel sequence
        for (int& id : channel->sequence) {
            push_bytes(out, (uint32_t) id);
        }

        // write pattern data
        for (Pattern* pattern : channel->patterns) {
            push_bytes(out, (uint32_t) pattern->notes.size());

            for (Note& note : pattern->notes) {
                push_bytes(out, (float) note.time);
                push_bytes(out, (int16_t) note.key);
                push_bytes(out, (float) note.length);
            }
        }
    }
}

Song* Song::from_file(std::istream& input, audiomod::ModuleOutputTarget& audio_out, std::string* error_msg) {
    // check if the magic number is valid
    char magic_number[4];
    input.read(magic_number, 4);
    if (memcmp("SnBx", magic_number, 4) != 0) return nullptr;

    uint8_t version[3];
    pull_bytes(input, version[0]);
    pull_bytes(input, version[1]);
    pull_bytes(input, version[2]);
    
    // get song name
    uint8_t song_name_size;
    pull_bytes(input, song_name_size);

    char* song_name = new char[song_name_size];
    input.read(song_name, song_name_size);

    // get song data
    uint32_t length, num_channels, max_patterns, beats_per_bar;
    pull_bytes(input, length);
    pull_bytes(input, num_channels);
    pull_bytes(input, max_patterns);
    pull_bytes(input, beats_per_bar);

    float tempo;
    pull_bytes(input, tempo);

    // create song
    Song* song = new Song(num_channels, length, max_patterns, audio_out);
    memcpy(song->name, song_name, song_name_size);
    song->name[song_name_size] = 0;
    song->beats_per_bar = beats_per_bar;
    song->tempo = tempo;

    delete[] song_name;

    // retrive channel data
    for (uint32_t channel_i = 0; channel_i < num_channels; channel_i++) {
        Channel* channel = song->channels[channel_i];

        // channel name
        uint8_t channel_name_size;
        pull_bytes(input, channel_name_size);

        input.read(channel->name, channel_name_size);
        channel->name[channel_name_size] = 0;

        // volume, panning
        float volume, panning;

        pull_bytes(input, volume);
        pull_bytes(input, panning);

        channel->vol_mod.volume = volume;
        channel->vol_mod.panning = panning;

        if (version[2] >= 2) {
            uint8_t channel_flags;
            pull_bytes(input, channel_flags);

            channel->vol_mod.mute = (channel_flags & 1) == 1;
            channel->solo =         (channel_flags & 2) == 2;
        }

        // instrument data
        if (version[2] >= 1) {
            // read instrument id
            uint8_t id_size;
            pull_bytes(input, id_size);

            char* inst_id = new char[id_size + 1];
            input.read(inst_id, id_size);
            inst_id[id_size] = 0;

            // load module based off id
            audiomod::ModuleBase* instrument = audiomod::create_module(inst_id);
            if (instrument == nullptr) {
                if (error_msg != nullptr) *error_msg = "unknown module type " + std::string(inst_id);
                delete[] inst_id;
                delete song;
                return nullptr;
            }
            channel->set_instrument(instrument);

            // load instrument state
            uint64_t inst_state_size;
            pull_bytes(input, inst_state_size);
            if (inst_state_size > 0) {
                uint8_t* inst_state = new uint8_t[inst_state_size];
                input.read((char*)inst_state, inst_state_size);
                channel->synth_mod->load_state(inst_state, inst_state_size);

                delete[] inst_state;
            }

            delete[] inst_id;
        }

        // get sequence
        uint32_t pattern_id;
        for (uint32_t i = 0; i < length; i++) {
            pull_bytes(input, pattern_id);
            channel->sequence[i] = pattern_id;
        }

        // get pattern and note data
        for (uint32_t pattern_i = 0; pattern_i < max_patterns; pattern_i++) {
            uint32_t num_notes;
            pull_bytes(input, num_notes);

            for (uint32_t i = 0; i < num_notes; i++) {
                float time, length;
                int16_t key;
                
                pull_bytes(input, time);
                pull_bytes(input, key);
                pull_bytes(input, length);

                channel->patterns[pattern_i]->notes.push_back({
                    time, key, length
                });
            }
        }
    }

    return song;
}