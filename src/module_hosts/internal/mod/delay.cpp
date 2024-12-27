#include "delay.hpp"
#include "audio_engine/audio_engine.hpp"
#include "dsp.hpp"
#include "imgui.h"
#include <imguiext/imgui-knobs.h>
#include "module_hosts/modules.hpp"

using namespace hosts::internal;

static constexpr float MAX_DELAY_SECS = 2.0f;

static const char* DIVISION_NAMES[] = {
    "1/64", // beats
    "1/64 dot", // dotted
    "1/48", // triplet
    "1/32", // beats
    "1/32 dot", // dotted
    "1/24", // triplet
    "1/16", // beats
    "1/16 dot", // dotted
    "1/12", // triplet
    "1/8", // beats
    "1/8 dot", // dotted
    "1/6", // triplet
    "1/4", // beats
    "1/4 dot", // dotted
    "1/3", // triplet
    "1/2", // beats
    "1/2 dot", // dotted
    "2/3", // triplet
    "1/1", // beats
    "1/1 dot", // dotted
    "3/3", // triplet
    "2/1", // beats
    "2/1 dot", // dotted
    "6/3", // triplet
    "4/1", // beats
    "4/1 dot", // dotted
    "12/3", // triplet
    "8/1",  // beats
    "8/1 dot", // dotted
    "24/3", // triplet
};

static float division_to_secs(float tempo, int division_enum)
{
    int exponent = division_enum / 3 - 6;
    int div_type = division_enum % 3;
    float len;

    if (div_type == 0) // normal beats
        len = powf(2.0f, exponent);
    else if (div_type == 1) // dotted beats
    {
        len = powf(2.0f, exponent);
        len += len / 2.0f;
    }
    else if (div_type == 2) // triplets
        len = powf(3.0f / 2.0f, exponent);

    return len * (60.0f / tempo);
}

DelayModule::DelayModule(modules::ModuleCreator &create) :
    modx::ModuleBase(create),
    event_reader(0)
{
    unsigned int sample_rate = create.engine.sample_rate();
    tempo = 0.0f;

    delay_line[0] = dsp::DelayLine<float>((size_t)(sample_rate * MAX_DELAY_SECS));
    delay_line[1] = dsp::DelayLine<float>((size_t)(sample_rate * MAX_DELAY_SECS));
    delay_line[0].delay = sample_rate * 0.5f;
    delay_line[1].delay = sample_rate * 0.5f;

    create.add_audio_input(2);
    create.add_audio_output(2);
    create.add_message_input();

    create.add_control<float>(CONTROL_FEEDBACK, "Feedback", 0.5f);
    create.add_control<float>(CONTROL_MIX, "Wet/Dry Mix", 0.5f);
    create.add_control<float>(CONTROL_DELAY_SECS_L, "Delay L (secs)", 0.3f);
    create.add_control<int>(CONTROL_DELAY_DIV_L, "Delay L (tempo division)", 0);
    create.add_control<float>(CONTROL_DELAY_SECS_R, "Delay R (secs)", 0.3f);
    create.add_control<int>(CONTROL_DELAY_DIV_R, "Delay R (tempo division)", 0);
    create.add_control<bool>(CONTROL_USE_TEMPO, "Use Tempo", false);
    create.add_control<bool>(CONTROL_DELAY_SYNC, "Delay Sync", false);
}

void DelayModule::process(modules::ModuleProcessor &proc) {
    constexpr int channel_count = 2;
    float *const input = proc.audio_input(0);
    float *const output = proc.audio_output(0);

    const float feedback = proc.get_control_value<float>(CONTROL_FEEDBACK);
    const float mix = proc.get_control_value<float>(CONTROL_MIX);
    const bool use_tempo = proc.get_control_value<bool>(CONTROL_USE_TEMPO);

    const int tempo_delay[2] = {
        proc.get_control_value<int>(CONTROL_DELAY_DIV_L),
        proc.get_control_value<int>(CONTROL_DELAY_DIV_R)
    };

    float delay_secs[2] = {
        proc.get_control_value<float>(CONTROL_DELAY_SECS_L),
        proc.get_control_value<float>(CONTROL_DELAY_SECS_R)
    };

    const float dryf = mix;
    const float wetf = 1.0f - mix;

    event_reader.new_run(&proc);

    if (use_tempo) {
        delay_secs[0] = division_to_secs(tempo, tempo_delay[0]);
        delay_secs[1] = division_to_secs(tempo, tempo_delay[1]);
    }

    delay_line[0].delay = (size_t)(proc.sample_rate * util::clamp(0.0f, MAX_DELAY_SECS, delay_secs[0]));
    delay_line[1].delay = (size_t)(proc.sample_rate * util::clamp(0.0f, MAX_DELAY_SECS, delay_secs[1]));

    for (size_t i = 0; i < proc.buffer_frame_count * channel_count; i += channel_count) {
        modx::TrackEvent ev;
        while (event_reader.read(&ev)) {
            if (ev.event_kind == modx::TrackEvent::TEMPO) {
                tempo = ev.tempo;
                
                if (use_tempo) {
                    delay_secs[0] = division_to_secs(tempo, tempo_delay[0]);
                    delay_secs[1] = division_to_secs(tempo, tempo_delay[1]);
                    delay_line[0].delay = (size_t)(proc.sample_rate * util::clamp(0.0f, MAX_DELAY_SECS, delay_secs[0]));
                    delay_line[1].delay = (size_t)(proc.sample_rate * util::clamp(0.0f, MAX_DELAY_SECS, delay_secs[1]));
                }
            }
        }
        
        float sample_l = delay_line[0].read();
        float sample_r = delay_line[1].read();

        output[i] = sample_l * wetf + input[i] * dryf;
        output[i+1] = sample_r * wetf + input[i+1] * dryf;

        delay_line[0].write(input[i] + sample_l * feedback);
        delay_line[1].write(input[i+1] + sample_r * feedback);
    }
}

void DelayModule::ui() {
    bool delay_sync = get_control_value<bool>(CONTROL_DELAY_SYNC);
    bool use_tempo = get_control_value<bool>(CONTROL_USE_TEMPO);

    // Feedback / Mix //
    ui_knob("Fdb", CONTROL_FEEDBACK, 0.0f, 1.0f, "%.2f");
    ImGui::SameLine();
    ui_knob("W/D", CONTROL_MIX, 0.0f, 1.0f, "%.2f");

    // Time knobs //
    if (use_tempo) { // tempo version
        int div_enum = -1;

        if (ui_knob_int("DlyL", CONTROL_DELAY_DIV_L, 0, 29, "%i", ImGuiKnobFlags_NoInput))
            if (delay_sync) set_control_value<int>(CONTROL_DELAY_DIV_R, get_control_value<int>(CONTROL_DELAY_DIV_L));
        
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) div_enum = get_control_value<int>(CONTROL_DELAY_DIV_L);
        
        ImGui::SameLine();
        if (ui_knob_int("DlyR", CONTROL_DELAY_DIV_R, 0, 29, "%i", ImGuiKnobFlags_NoInput))
            if (delay_sync) set_control_value<int>(CONTROL_DELAY_DIV_L, get_control_value<int>(CONTROL_DELAY_DIV_R));

        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) div_enum = get_control_value<int>(CONTROL_DELAY_DIV_R);
        
        if (div_enum >= 0) {
            ImGui::TextDisabled("%s", DIVISION_NAMES[div_enum]);
        } else {
            ImGui::NewLine();
        }
    }
    else // secs version
    {
        if (ui_knob("DlyL", CONTROL_DELAY_SECS_L, 0.0f, MAX_DELAY_SECS, "%.2fs"))
            if (delay_sync) set_control_value<float>(CONTROL_DELAY_SECS_R, get_control_value<float>(CONTROL_DELAY_SECS_L));
        
        ImGui::SameLine();
        if (ui_knob("DlyR", CONTROL_DELAY_SECS_R, 0.0f, MAX_DELAY_SECS, "%.2fs"))
            if (delay_sync) set_control_value<float>(CONTROL_DELAY_SECS_L, get_control_value<float>(CONTROL_DELAY_SECS_R));
    }

    // delay options //
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2());
    {
        ImGui::Text("Sync");
        ImGui::SameLine();
        if (ImGui::Checkbox("##Sync", &delay_sync))
            set_control_value<bool>(CONTROL_DELAY_SYNC, delay_sync);

        ImGui::Text("Tempo");
        ImGui::SameLine();
        if (ImGui::Checkbox("##Use Tempo", &use_tempo))
            set_control_value<bool>(CONTROL_USE_TEMPO, use_tempo);
        ImGui::PopStyleVar();
    }
}