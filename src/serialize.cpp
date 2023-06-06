#include "sys.h"
#include "song.h"
#include <cstdint>

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
    push_bytes(out, (uint8_t)3);

    // write song data
    push_bytes(out, (uint8_t) strlen(name));
    out << name;

    push_bytes(out, (uint32_t) length());
    push_bytes(out, (uint32_t) channels.size());
    push_bytes(out, (uint32_t) max_patterns());
    push_bytes(out, (uint32_t) beats_per_bar);
    push_bytes(out, (float) tempo);

    // v3: tuning data
    push_bytes(out, (uint8_t) (tunings.size() - 1));
    for (Tuning* tuning : tunings)
    {
        // ignore first entry (12edo)
        if (!tuning->is_12edo)
        {
            // write name
            push_bytes(out, (uint32_t) tuning->name.size());
            out << tuning->name;
            
            // write description
            push_bytes(out, (uint32_t) tuning->desc.size());
            out << tuning->desc;

            // write key frequencies
            push_bytes(out, (uint32_t) tuning->key_freqs.size());
            for (float freq : tuning->key_freqs)
            {
                push_bytes(out, (float) freq);
            }
        }
    }

    // write the selected tuning
    push_bytes(out, (uint8_t) selected_tuning);

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

    // v3: tuning data
    if (version[2] >= 3)
    {
        uint8_t num_tunings;
        pull_bytes(input, num_tunings);

        // read tunings
        for (uint8_t i = 0; i < num_tunings; i++)
        {
            Tuning* tuning = new Tuning();
            uint32_t name_size, desc_size, keys_size;
            float freq;

            // read name
            pull_bytes(input, name_size);
            tuning->name.resize(name_size);
            input.read(&tuning->name.front(), name_size);

            // read description
            pull_bytes(input, desc_size);
            tuning->desc.resize(desc_size);
            input.read(&tuning->desc.front(), desc_size);

            // get key frequencies
            pull_bytes(input, keys_size);
            for (uint32_t i = 0; i < keys_size; i++)
            {
                pull_bytes(input, freq);
                tuning->key_freqs.push_back(freq);
            }

            tuning->analyze();
            song->tunings.push_back(tuning);
        }

        // selected tuning
        uint8_t selected_tuning_uint8;
        pull_bytes(input, selected_tuning_uint8);
        song->selected_tuning = selected_tuning_uint8;
    }

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