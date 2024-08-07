#include "midi.hpp"

bool midi::is_note_off(MidiEvent event, uint8_t *channel)
{
    if ((event.status & 0b11110000) == MidiStatus::STATUS_NOTE_OFF)
    {
        if (channel != nullptr)
            *channel = event.status & MidiStatus::STATUS_CHANNEL_MASK;
        return true;
    }
    return false;
}

bool midi::is_note_on(MidiEvent event, uint8_t *channel)
{
    if ((event.status & 0b11110000) == MidiStatus::STATUS_NOTE_ON)
    {
        if (channel != nullptr)
            *channel = event.status & MidiStatus::STATUS_CHANNEL_MASK;
        return true;
    }
    return false;
}