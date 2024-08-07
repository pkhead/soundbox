#include <cstdint>
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

midi::MidiEvent midi::note_off(uint8_t channel, uint8_t key, uint8_t velocity)
{
    midi::MidiEvent event{};
    event.status = MidiStatus::STATUS_NOTE_OFF | (channel | MidiStatus::STATUS_CHANNEL_MASK);
    event.note.key = key;
    event.note.velocity = velocity;
    return event;
}

midi::MidiEvent midi::note_on(uint8_t channel, uint8_t key, uint8_t velocity)
{
    midi::MidiEvent event{};
    event.status = MidiStatus::STATUS_NOTE_ON | (channel | MidiStatus::STATUS_CHANNEL_MASK);
    event.note.key = key;
    event.note.velocity = velocity;
    return event;
}