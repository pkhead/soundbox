/*
Contains implementations for the following modules:
 - sbox::midi_in
 - sbox::fader
*/

#include <cfloat>
#include <cmath>
#include <cassert>
#include <cstdio>
#include <imgui.h>
#include <imguiext/imgui-knobs.h>
#include "dsp.h"
#include "modules.hpp"
#include "midi.hpp"
#include "ui.hpp"

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
    // setup module i/o and controls
    create.add_message_input();
    create.add_audio_output(2);

    for (unsigned int i = 0; i < OSC_COUNT; i++)
    {
        char buf[64];
        unsigned int control_start = OSC_CONTROL_START[i];

        // osc type
        sprintf(buf, "Oscillator %i Type", i);
        create.add_control<int>(control_start + CONTROL_OSC_TYPE, buf, WAVE_SQUARE);

        // osc vol
        sprintf(buf, "Oscillator %i Volume", i);
        create.add_control<float>(control_start + CONTROL_OSC_VOL, buf, 0.5f);

        // osc pan
        sprintf(buf, "Oscillator %i Panning", i);
        create.add_control<float>(control_start + CONTROL_OSC_PAN, buf, 0.0f);

        // osc coarse
        sprintf(buf, "Oscillator %i Coarse Detune", i);
        create.add_control<int>(control_start + CONTROL_OSC_COARSE, buf, 0);

        // osc fine
        sprintf(buf, "Oscillator %i Fine Detune", i);
        create.add_control<float>(control_start + CONTROL_OSC_FINE, buf, 0.0f);
    }
    
    create.add_control<float>(CONTROL_AMP_ATTACK, "Amplitude Envelope Attack", 0.0f);
    create.add_control<float>(CONTROL_AMP_SUSTAIN, "Amplitude Envelope Sustain", 1.0f);
    create.add_control<float>(CONTROL_AMP_DECAY, "Amplitude Envelope Decay", 0.0f);
    create.add_control<float>(CONTROL_AMP_RELEASE, "Amplitude Envelope Release", 0.0f);

    create.add_control<float>(CONTROL_FILTER_ATTACK, "Filter Envelope Attack", 0.0f);
    create.add_control<float>(CONTROL_FILTER_SUSTAIN, "Filter Envelope Sustain", 1.0f);
    create.add_control<float>(CONTROL_FILTER_DECAY, "Filter Envelope Decay", 0.0f);
    create.add_control<float>(CONTROL_FILTER_RELEASE, "Filter Envelope Release", 0.0f);

    create.add_control<int>(CONTROL_FILTER_TYPE, "Filter Type", FILTER_LOW_PASS);
    create.add_control<float>(CONTROL_FILTER_FREQ, "Filter Frequency", (float)create.engine.sample_rate() * 0.35f);
    create.add_control<float>(CONTROL_FILTER_RESO, "Filter Resonance", 1.0f);
    create.add_control<float>(CONTROL_FILTER_ENV, "Filter Resonance", 0.0f);

    // setup internal module state
    for (int i = 0; i < MAX_VOICES; i++)
    {
        voices[i].phase = 0.0f;
        voices[i].active = false;
    }

    ui_selected_osc = 0;
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
    ImGui::SeparatorText("Simple Oscillator");

    float knob_size = ImGui::GetFontSize() * 3.0f;
    ImGuiKnobFlags knob_flags = 0;

    ImGuiChildFlags group_child_flags = ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_Border;
    
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::SliderInt("##SelectedOsc", &ui_selected_osc, 1, 3, "%d", ImGuiSliderFlags_AlwaysClamp);

    {
        int i = ui_selected_osc - 1;

        ImGui::BeginChild("osc", ImVec2(-FLT_MIN, 0.0f), group_child_flags);
        ImGui::TextDisabled("Oscillator %i", i+1);

        // waveform combobox
        int waveform = engine->control_get_value<int>(id(), OSC_CONTROL_START[i] + CONTROL_OSC_TYPE);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Waveform");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8.0f);
        if (ImGui::Combo("##waveform", &waveform, "Square\0Sawtooth\0Pulse\0Triangle\0Sine\0"))
            engine->control_set_value<int>(id(), OSC_CONTROL_START[i] + CONTROL_OSC_TYPE, waveform);

        // control knobs
        ui_knob("Vol", OSC_CONTROL_START[i] + CONTROL_OSC_VOL, 0.0f, 100.0f, "%.0f");
        ImGui::SameLine();
        ui_knob("Pan", OSC_CONTROL_START[i] + CONTROL_OSC_PAN, -1.0f, 1.0f, "%.2f");
        ImGui::SameLine();
        ui_knob_int("Crs", OSC_CONTROL_START[i] + CONTROL_OSC_COARSE, -24, 24, "%i st");
        ImGui::SameLine();
        ui_knob("Fine", OSC_CONTROL_START[i] + CONTROL_OSC_FINE, -100.0f, 100.0f, "%.0fc");
        ImGui::EndChild();
    }

    // filter options
    ImGui::BeginChild("filter", ImVec2(-FLT_MIN, 0.0f), group_child_flags);
    {
        ImGui::TextDisabled("Filter");

        // filter type combobox
        int filter_type = engine->control_get_value<int>(id(), CONTROL_FILTER_TYPE);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Type");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8.0f);
        if (ImGui::Combo("##filter_type", &filter_type, "Low Pass\0High Pass\0"))
            engine->control_set_value<int>(id(), CONTROL_FILTER_TYPE, filter_type);

        ImGuiKnobFlags flags = ImGuiKnobFlags_NoInput;
        ui_knob("Freq", CONTROL_FILTER_FREQ, 20.0f, engine->sample_rate() * 0.4f, "%.3f", flags);
        ImGui::SameLine();
        ui_knob("Reso", CONTROL_FILTER_RESO, 0.1f, 2.0f, "%.3f", flags);
        ImGui::SameLine();
        ui_knob("Env", CONTROL_FILTER_ENV, 0.0f, 1.0f, "%.3f", flags);
    }
    ImGui::EndChild();

    // amplitude envelope
    {
        ui::adsr_ui_struct adsr_struct(
            get_control_value<float>(CONTROL_AMP_ATTACK),
            get_control_value<float>(CONTROL_AMP_DECAY),
            get_control_value<float>(CONTROL_AMP_SUSTAIN),
            get_control_value<float>(CONTROL_AMP_RELEASE)
        );

        if (ui::adsr_ui("Amplitude Envelope", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight() * 2.0f), &adsr_struct))
        {
            set_control_value<float>(CONTROL_AMP_ATTACK, adsr_struct.attack);
            set_control_value<float>(CONTROL_AMP_DECAY, adsr_struct.decay);
            set_control_value<float>(CONTROL_AMP_SUSTAIN, adsr_struct.sustain);
            set_control_value<float>(CONTROL_AMP_RELEASE, adsr_struct.release);
        }
    }

    // filter envelope
    {
        ui::adsr_ui_struct adsr_struct(
            get_control_value<float>(CONTROL_FILTER_ATTACK),
            get_control_value<float>(CONTROL_FILTER_DECAY),
            get_control_value<float>(CONTROL_FILTER_SUSTAIN),
            get_control_value<float>(CONTROL_FILTER_RELEASE)
        );

        if (ui::adsr_ui("Filter Envelope", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight() * 2.0f), &adsr_struct))
        {
            set_control_value<float>(CONTROL_FILTER_ATTACK, adsr_struct.attack);
            set_control_value<float>(CONTROL_FILTER_DECAY, adsr_struct.decay);
            set_control_value<float>(CONTROL_FILTER_SUSTAIN, adsr_struct.sustain);
            set_control_value<float>(CONTROL_FILTER_RELEASE, adsr_struct.release);
        }
    }

    /*ImGui::BeginChild("ampenv", ImVec2(-FLT_MIN, 0.0f), group_child_flags);
    ImGui::TextDisabled("Amplitude Envelope");
    ui_knob("Atk", CONTROL_AMP_ATTACK, 0.0f, 10.0f, 0.0f, "%.2f s");
    ImGui::SameLine();
    ui_knob("Dky", CONTROL_AMP_DECAY, 0.0f, 10.0f, 0.0f, "%.2f s");
    ImGui::SameLine();
    ui_knob("Sus", CONTROL_AMP_SUSTAIN, 0.0f, 1.0f, 0.0f, "%.2f s");
    ImGui::SameLine();
    ui_knob("Rls", CONTROL_AMP_RELEASE, 0.0f, 10.0f, 0.0f, "%.2f s");
    ImGui::EndChild();*/
}    

