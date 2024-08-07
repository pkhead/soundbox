#pragma once
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
        MidiStatus status;

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

    bool is_note_on(MidiEvent event, uint8_t *channel = nullptr);
    bool is_note_off(MidiEvent event, uint8_t *channel = nullptr);
}