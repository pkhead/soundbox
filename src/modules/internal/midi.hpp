#pragma once
#include <algorithm>
#include <cstdint>

namespace midi
{
    enum MidiStatus : uint8_t
    {
        STATUS_CHANNEL_MASK = 0b00001111,
        STATUS_NOTE_OFF = 0b10000000,
        STATUS_NOTE_ON = 0b10010000,
    };

    struct MidiEvent
    {
        uint8_t status;

        union
        {
            uint8_t data[2];

            struct
            {
                uint8_t key;
                uint8_t velocity;
            } note;
        };
    };

    MidiEvent note_on(uint8_t channel, uint8_t key, uint8_t velocity);
    inline MidiEvent note_on(uint8_t channel, uint8_t key, float velocity) {
        return note_on(channel, key, (uint8_t)(std::clamp(velocity, 0.0f, 1.0f) * 127.0f));
    }

    MidiEvent note_off(uint8_t channel, uint8_t key, uint8_t velocity);
    inline MidiEvent note_off(uint8_t channel, uint8_t key, float velocity) {
        return note_off(channel, key, (uint8_t)(std::clamp(velocity, 0.0f, 1.0f) * 127.0f));
    }

    bool is_note_on(MidiEvent event, uint8_t *channel = nullptr);
    bool is_note_off(MidiEvent event, uint8_t *channel = nullptr);
}