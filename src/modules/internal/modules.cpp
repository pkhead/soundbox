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

///////////////
// sbox::osc //
///////////////

OscModule::OscModule(modules::ModuleCreator &create) : modx::ModuleBase(create)
{
    create.add_message_input();
    create.add_audio_output(2);

    for (int i = 0; i < MAX_VOICES; i++)
    {
        voices[i].phase = 0.0f;
        voices[i].active = false;
    }
}

static float poly_blep(float t, float inc)
{
    float dt = inc / (2 * M_PI);
    // 0 <= t < 1
    if (t < dt) {
        t /= dt;
        return t+t - t*t - 1.0;
    }
    // -1 < t < 0
    else if (t > 1.0 - dt) {
        t = (t - 1.0) / dt;
        return t*t + t+t + 1.0;
    }
    // 0 otherwise
    else return 0.0;
}

void OscModule::process(modules::ModuleProcessor &proc)
{
    // process midi input
    midi::MidiEvent midi_event;

    while (true)
    {
        unsigned int read = proc.read_message(0, &midi_event, sizeof(midi_event));
        if (read == 0) break;
        assert(read == sizeof(midi_event));
        
        if (midi::is_note_on(midi_event))
        {
            logger::log_debug("note on key %i", midi_event.note.key);
            for (int i = 0; i < MAX_VOICES; i++)
            {
                if (!voices[i].active)
                {
                    voices[i].key = midi_event.note.key;
                    voices[i].freq = powf(2.0f, (float)(midi_event.note.key - 69) / 12.0f) * 440.0f;
                    voices[i].volume = (float)midi_event.note.velocity / 127.0f;
                    voices[i].active = true;
                    break;
                }
            }
        }
        else if (midi::is_note_off(midi_event))
        {
            logger::log_debug("note off key %i", midi_event.note.key);
            for (int i = 0; i < MAX_VOICES; i++)
            {
                if (voices[i].active && voices[i].key == midi_event.note.key)
                {
                    voices[i].active = false;
                    break;
                }
            }
        }
    }

    float* out_samples = proc.audio_output(0);
    unsigned int channel_count = proc.audio_output_channels(0);

    float vol = 0.4f;

    for (unsigned int i = 0; i < proc.buffer_frame_count; i++)
    {
        float sample = 0.0f;
        for (int i = 0; i < MAX_VOICES; i++)
        {
            if (!voices[i].active) continue;

            float increment = (2.0f * M_PIf * voices[i].freq) / proc.sample_rate;
            sample += voices[i].phase < M_PIf ? 1.0f : -1.0f;
            sample += poly_blep(voices[i].phase / (2.0f * M_PIf), increment);
            sample -= poly_blep(fmod(voices[i].phase / (2.0f * M_PIf) + 0.5f, 1.0f), increment);

            //sample += sin(voices[i].phase);
            sample *= voices[i].volume;
            voices[i].phase += increment;
            if (voices[i].phase >= 2.0f * M_PIf)
                voices[i].phase -= 2.0f * M_PIf;
        }

        for (unsigned int j = 0; j < channel_count; j++)
            *out_samples++ = sample * vol;
    }
}

void OscModule::ui()
{
    // no-op
}

