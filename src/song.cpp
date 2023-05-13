#include <assert.h>
#include <string.h>
#include "song.h"

Pattern::Pattern() { };

Note& Pattern::add_note(float time, int key, float length) {
    notes.push_back({ time, key, length });
    return *notes.end();
}

inline bool Pattern::is_empty() const {
    return notes.empty();
}

/*************************
*        CHANNEL         *
*************************/
Channel::Channel(int song_length, int max_patterns) : volume(0.5f), panning(0.0f) {
    strcpy(name, "Channel");
    
    for (int i = 0; i < song_length; i++) {
        sequence.push_back(0);
    }

    for (int i = 0; i < max_patterns; i++) {
        Pattern* pattern = new Pattern();
        patterns.push_back(pattern);

        pattern->add_note(0.0f, 58, 2.0f);
    }
}

Channel::~Channel() {
    for (Pattern* pattern : patterns) {
        delete pattern;
    }
}


/*************************
*         SONG           *
*************************/

Song::Song(int num_channels, int length, int max_patterns) : _length(length), _max_patterns(max_patterns) {
    for (int ch_i = 0; ch_i < num_channels; ch_i++) {
        Channel* ch = new Channel(_length, _max_patterns);
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