/*
Contains implementations for all "control" modules:
 - sbox::midi_in
 - sbox::fader
*/

#include <cfloat>
#include <cmath>
#include <cassert>
#include "../modules.hpp"
#include "../midi.hpp"

using namespace hosts::internal;

///////////////////
// sbox::midi_in //
///////////////////
constexpr size_t MIDI_QUEUE_SIZE = sizeof(midi::MidiEvent) * 32;

MidiInputModule::MidiInputModule(modules::ModuleCreator &create) :
    modx::ModuleBase(create),
    midi_queue(MIDI_QUEUE_SIZE)
{
    create.add_message_output();
}

void MidiInputModule::process(modules::ModuleProcessor &proc)
{
    std::byte msg_buf[sizeof(midi::MidiEvent)];

    while (midi_queue.read(msg_buf, sizeof(midi::MidiEvent)))
    {
        proc.send_message(0, (std::byte*) &msg_buf, sizeof(midi::MidiEvent));
    }
}

/////////////////
// sbox::fader //
/////////////////
FaderModule::FaderModule(modules::ModuleCreator &create) : modx::ModuleBase(create)
{
    create.add_control<float>(FADER_CONTROL_GAIN, "gain", 0.0f);
    create.add_control<float>(FADER_CONTROL_PAN, "pan", 0.0f);
    create.add_control<bool>(FADER_CONTROL_MUTE, "mute", false);

    create.add_audio_input(2);
    create.add_audio_output(2);
}

void FaderModule::process(modules::ModuleProcessor &proc)
{
    float *audio_in = proc.audio_input(0);
    float *audio_out = proc.audio_output(0);

    assert(proc.audio_input_channels(0) == 2);
    assert(proc.audio_output_channels(0) == 2);

    float gain = proc.get_control_value<float>(FADER_CONTROL_GAIN);
    float pan = proc.get_control_value<float>(FADER_CONTROL_PAN);
    bool mute = proc.get_control_value<bool>(FADER_CONTROL_MUTE);
    
    float linear_gain = db_to_mult(gain);
    float right = (pan + 1.0f) / 2.0f;
    float left = 1.0f - right;

    if (mute) linear_gain = 0.0f;

    for (size_t i = 0; i < proc.buffer_frame_count; i++)
    {
        *audio_out++ = (*audio_in++) * left * linear_gain;
        *audio_out++ = (*audio_in++) * right * linear_gain;
    }
}