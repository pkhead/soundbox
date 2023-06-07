#include <algorithm>
#include <assert.h>
#include <cctype>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ios>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string.h>
#include <math.h>
#include <string>

#include <TUN_Scale.h>
#include "TUN_StringTools.h"
#include "audio.h"
#include "imgui.h"
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
    synth_mod->parent_name = name;
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

void Channel::set_fx_target(int fx_index)
{
    // disconnect old target
    fx_mixer[fx_target_idx]->disconnect_input(&vol_mod);

    fx_target_idx = fx_index;
    fx_mixer[fx_index]->connect_input(&vol_mod);
}

/*************************
*        TUNING          *
*************************/

static long gcd(long a, long b)
{
    if (a == 0)
        return b;
    else if (b == 0)
        return a;

    if (a < b)
        return gcd(a, b % a);
    else
        return gcd(b, a % b);
}

void Tuning::analyze()
{
    key_info.clear();

    // number of the current octave
    int octave_number = -1;

    // frequency of the base note of this octave
    double octave_freq = -9999;
    
    for (float freq : key_freqs)
    {
        KeyInfoStruct info = {
            false,
            false,
            octave_number,
            0.0f,
            0,
            1,
            0
        };

        // if this is the first note, then mark this note as an octave key
        if (octave_freq == -9999)
        {
            info.is_octave = true;
            octave_freq = freq;
            octave_number++;
        }
        else
        {
            // if a 2/1 interval was found, the next octave was found
            if (octave_freq * 2.0 == freq)
            {
                info.is_octave = true;
                octave_freq = freq;
                octave_number++;
            }

            // if a 3/2 interval was found, this is a fifth
            else if (std::abs(freq / octave_freq - 1.5) < 0.025)
            {
                info.is_fifth = true;
            }
        }

        info.ratio_float = freq / octave_freq;
        info.octave_number = octave_number;
        
        // calculate ratio numerator/denominator
        constexpr long PRECISION = 16;
        long num = freq / octave_freq * PRECISION;
        long den = PRECISION;
        long div = gcd(num, den);

        info.ratio_num = num / div;
        info.ratio_den = den / div;

        key_info.push_back(info);
    }

    // now, record the color of each note
    // it will be a rainbow that wraps around every octave
    int cur_octave = 0;
    int next_octave = 0;

    for (size_t key = 0; key < key_freqs.size(); key++)
    {
        // find the next octave
        if (key == next_octave)
        {
            cur_octave = next_octave;
            next_octave = -1; // set identifier for no octave in case none is found

            for (int k = key + 1; k < key_freqs.size(); k++)
            {
                if (key_info[k].is_octave)
                {
                    next_octave = k;
                    break;
                }
            }
        }

        // if this scale has octaves, repeat colors every octave
        // otherwise repeat colors an arbitrary amount so that
        // it's easier to keep track of where you are in the
        // piano roll
        float repeat_len;
        if (next_octave == -1)
            repeat_len = 16;
        else
            repeat_len = next_octave - cur_octave;

        float r, g, b;
        ImGui::ColorConvertHSVtoRGB(
            float(key - cur_octave) / repeat_len,
            0.5f,
            0.4f,
            r, g, b
        );

        key_info[key].key_color = IM_COL32(r * 255.0f, g * 255.0f, b * 255.0f, 255.0f);
    }
}

/*************************
*         SONG           *
*************************/

Song::Song(int num_channels, int length, int max_patterns, audiomod::ModuleOutputTarget& audio_out) : audio_out(audio_out), _length(length), _max_patterns(max_patterns) {
    strcpy(name, "Untitled");

    // create 12edo scale
    Tuning* tuning = new Tuning();
    tuning->name = "12edo";
    tuning->desc = "The standard tuning system";
    tuning->is_12edo = true;
    
    // generate 12edo
    for (int key = 0; key < 128; key++)
        tuning->key_freqs.push_back( powf(2.0f, (key - 57) / 12.0f) * 440.0f );
    
    tuning->analyze();
    tunings.push_back(tuning);

    audiomod::FXBus* master_bus = new audiomod::FXBus();
    strcpy(master_bus->name, "Master");
    master_bus->target_bus = 0;
    master_bus->connect_output(&audio_out);
    fx_mixer.push_back(master_bus);

    for (int i = 0; i < 3; i++)
    {
        audiomod::FXBus* bus = new audiomod::FXBus();
        snprintf(bus->name, bus->name_capacity, "Bus %i", i);
        bus->connect_output(&audio_out);
        fx_mixer.push_back(bus);

        master_bus->connect_input(&bus->controller);
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
    Tuning* tuning = tunings[selected_tuning];

    if (key < 0) return 0.00001f;
    if (key >= tuning->key_freqs.size()) return 0.00001f;

    return tuning->key_freqs[key];
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
        if (*it == mod) {
            mod_interfaces.erase(it);
            break;
        }
    }
}

void Song::toggle_module_interface(audiomod::ModuleBase* mod) {
    mod->show_interface = !mod->show_interface;
    
    // if want to show interface, add module to interfaces list
    if (mod->show_interface) {
        mod_interfaces.push_back(mod);

    // if want to hide interface, remove module from interfaces list
    } else {
        hide_module_interface(mod);
    }
}

static std::string& string_trim(std::string& str)
{
    // trim the start
    bool all_whitespace = true;

    for (auto it = str.begin(); it != str.end(); it++) {
        if (!std::isspace(*it)) {
            all_whitespace = false;
            str.erase(str.begin(), it);
            break;
        }
    }

    // no non-whitespace character was found, so the entire string is whitespace
    // so the entire string must be cleared
    if (all_whitespace)
    {
        str.clear();
        return str;
    }

    // trim the end
    for (auto it = str.rbegin(); it != str.rend(); it++) {
        if (!std::isspace(*it)) {
            str.erase(it.base(), str.end());
            break;
        }
    }

    return str;
}

Tuning* Song::load_scale_tun(std::istream& input, std::string* err)
{
    TUN::CStringParser strparser;
    TUN::CSingleScale scale;
    Tuning* tuning = new Tuning();
    strparser.InitStreamReading();
    
    if (scale.Read(input, strparser) == 1)
    {   // if read was successful
        tuning->desc = scale.m_strDescription;

        for (int midi_key : scale.GetMapping())
        {
            float freq = scale.GetMIDINoteFreqHz(midi_key);
            tuning->key_freqs.push_back(freq);
        }

        tuning->analyze();
        tunings.push_back(tuning);
        return tuning;
    }
    else
    {   // if there was an error
        if (err) *err = scale.Err().GetLastError();
        delete tuning;
        return nullptr;
    }
}