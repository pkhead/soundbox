#include <assert.h>
#include <string.h>
#include <math.h>
#include "song.h"

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
Channel::Channel(int song_length, int max_patterns, audiomod::ModuleOutputTarget& audio_out) : audio_out(audio_out), volume(0.5f), panning(0.0f) {
    strcpy(name, "Channel");
    
    for (int i = 0; i < song_length; i++) {
        sequence.push_back(0);
    }

    for (int i = 0; i < max_patterns; i++) {
        patterns.push_back(new Pattern());
    }

    synth_mod.connect(&vol_mod);
    vol_mod.connect(&audio_out);
}

Channel::~Channel() {
    for (Pattern* pattern : patterns) {
        delete pattern;
    }
}


/*************************
*         SONG           *
*************************/

Song::Song(int num_channels, int length, int max_patterns, audiomod::ModuleOutputTarget& audio_out) : audio_out(audio_out), _length(length), _max_patterns(max_patterns) {
    for (int ch_i = 0; ch_i < num_channels; ch_i++) {
        Channel* ch = new Channel(_length, _max_patterns, audio_out);
        sprintf(ch->name, "Channel %i", ch_i + 1);
        channels.push_back(ch);
    }
}

Song::~Song() {
    for (Channel* channel : channels) {
        delete channel;
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

float Song::get_key_frequency(int key) const {
    return powf(2.0f, (key - 57) / 12.0f) * 440.0;
}

void Song::play() {
    is_playing = true;
    position = bar_position * beats_per_bar;

    prev_notes.clear();
}

void Song::stop() {
    is_playing = false;

    for (NoteData note_data : cur_notes) {
        channels[note_data.channel_i]->synth_mod.event({
            audiomod::NoteEventKind::NoteOff,
            note_data.note.key
        });
    }

    prev_notes.clear();
    cur_notes.clear();
}

void Song::update(double elapsed) {
    if (is_playing) {
        position += elapsed * (tempo / 60.0);
        if (position >= _length * beats_per_bar) position -= _length * beats_per_bar;
        bar_position = position / beats_per_bar;

        // get notes at playhead
        float pos_in_bar = fmod(position, (double)beats_per_bar);
        cur_notes.clear();

        int channel_i = 0;
        for (Channel* channel : channels) {
            int pattern_index = channel->sequence[bar_position] - 1;
            if (pattern_index >= 0) {
                Pattern* pattern = channel->patterns[pattern_index];

                for (Note& note : pattern->notes) {
                    if (pos_in_bar > note.time && pos_in_bar < note.time + note.length) cur_notes.push_back({
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
                channels[new_note.channel_i]->synth_mod.event({
                    audiomod::NoteEventKind::NoteOn,
                    {
                        new_note.note.key,
                        get_key_frequency(new_note.note.key),
                        0.2f
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
                channels[old_note.channel_i]->synth_mod.event({
                    audiomod::NoteEventKind::NoteOff,
                    old_note.note.key
                });
            }
        }

        prev_notes = cur_notes;
    }
}