/*
Contains implementations for the following modules:
 - sbox::midi_in
 - sbox::fader
*/

#include <cmath>
#include <cassert>
#include "dsp.h"
#include "modules.hpp"
#include "midi.hpp"

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
    std::byte msg_buf[MIDI_QUEUE_SIZE];

    while (true)
    {
        unsigned int read = proc.read_message(0, msg_buf, MIDI_QUEUE_SIZE);
        if (read == 0) break;
        proc.send_message(0, msg_buf, read);
    }
}

/////////////////
// sbox::fader //
/////////////////
enum FaderControl
{
    FADER_CONTROL_GAIN,
    FADER_CONTROL_PAN
};

FaderModule::FaderModule(modules::ModuleCreator &create) : modx::ModuleBase(create)
{
    create.add_control<float>(FADER_CONTROL_GAIN, "gain", 0.0f);
    create.add_control<float>(FADER_CONTROL_PAN, "pan", 0.0f);

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
    
    float linear_gain = db_to_mult(gain);
    float right = (pan + 1.0f) / 2.0f;
    float left = 1.0f - right;

    for (size_t i = 0; i < proc.buffer_frame_count; i++)
    {
        *audio_out++ = (*audio_in++) * left * linear_gain;
        *audio_out++ = (*audio_in++) * right * linear_gain;
    }
}

///////////////
// sbox::osc //
///////////////

OscModule::OscModule(modules::ModuleCreator &create) : modx::ModuleBase(create)
{
    phase = 0.0f;
    freq = 440.0f;

    create.add_audio_output(2);
}

void OscModule::process(modules::ModuleProcessor &proc)
{
    uint8_t channels = proc.audio_output_channels(0);
    float *output = proc.audio_output(0);

    for (size_t i = 0; i < proc.buffer_frame_count; i++)
    {
        float sample = sinf(phase) * 0.4f;

        phase += (2.0f * M_PIf * freq) / proc.sample_rate;
        if (phase >= 2.0f * M_PIf)
            phase -= 2.0f * M_PIf;

        for (uint8_t j = 0; j < channels; j++)
        {
            *output++ = sample;
        }
    }
}

void OscModule::ui()
{
    // no-op
}

