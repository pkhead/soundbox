/*
Contains implementations for all "control" modules:
 - sbox::midi_in
 - sbox::fader
 - sbox::gain
*/

#include <cfloat>
#include <cmath>
#include <cassert>
#include <numutil.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <dsp.hpp>
#include <audio_engine/audio_engine.hpp>
#include "../modules.hpp"
#include "../host.hpp"
#include "modules/modules.hpp"

using namespace hosts::internal;

///////////////////
// sbox::midi_in //
///////////////////
constexpr size_t MIDI_QUEUE_SIZE = sizeof(modx::TrackEvent) * 32;

MidiInputModule::MidiInputModule(modules::ModuleCreator &create) :
    modx::ModuleBase(create),
    midi_queue(MIDI_QUEUE_SIZE)
{
    create.add_message_output();
}

void MidiInputModule::process(modules::ModuleProcessor &proc)
{
    modx::TrackEvent buf;

    while (midi_queue.read((std::byte*) &buf, sizeof(buf)))
    {
        proc.send_message(0, (std::byte*) &buf, sizeof(buf));
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
    
    float linear_gain = dsp::db_to_mult(gain);
    float right = (pan + 1.0f) / 2.0f;
    float left = 1.0f - right;

    if (mute) linear_gain = 0.0f;

    for (size_t i = 0; i < proc.buffer_frame_count; i++)
    {
        *audio_out++ = (*audio_in++) * left * linear_gain;
        *audio_out++ = (*audio_in++) * right * linear_gain;
    }
}

////////////////
// sbox::gain //
////////////////
GainModule::GainModule(modules::ModuleCreator &creator) : modx::ModuleBase(creator)
{
    creator.name = "Gain";
    creator.add_audio_input(2);
    creator.add_audio_output(2);
    creator.add_control<float>(0, "Gain", 0.0f);
}

void GainModule::process(modules::ModuleProcessor &proc)
{
    float *in = proc.audio_input(0);
    float *out = proc.audio_output(0);
    float gain = proc.get_control_value<float>(0);
    float linear_gain = dsp::db_to_mult(gain);

    assert(proc.audio_input_channels(0) == 2);
    assert(proc.audio_output_channels(0) == 2);

    for (unsigned int i = 0; i < proc.buffer_frame_count; i++)
    {
        *out++ = *in++ * linear_gain;
        *out++ = *in++ * linear_gain;
    }
}

void GainModule::ui()
{
    float gain = get_control_value<float>(0);

    // center slider
    float slider_width = ImGui::GetFrameHeight() * 1.2f;
    ImGui::SetCursorPos(Vec2(ImGui::GetCursorPos()) + Vec2(((ImGui::GetContentRegionAvail().x - (slider_width)) / 2.0f), 0.0f));
    //ImGui::SameLine((ImGui::GetContentRegionAvail().x - (slider_width + ImGui::GetStyle().ItemSpacing.x)) / 2.0f);

    bool is_changed = ImGui::VSliderFloat("##Gain", ImVec2(slider_width, ImGui::GetContentRegionAvail().y), &gain, -20.0f, 20.0f, "");

    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
        ImGui::SetTooltip("%.2f dB", gain);

    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle))
    {
        gain = 0.0f;
        is_changed = true;
    }

    if (is_changed)
            set_control_value<float>(0, gain);
}