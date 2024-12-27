#include "delay.hpp"
#include "audio_engine/audio_engine.hpp"
#include "dsp.hpp"
#include "imgui.h"
#include "module_hosts/modules.hpp"

using namespace hosts::internal;

static constexpr float MAX_DELAY_SECS = 2.0f;

DelayModule::DelayModule(modules::ModuleCreator &create) :
    modx::ModuleBase(create),
    event_reader(0)
{
    unsigned int sample_rate = create.engine.sample_rate();

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
    float delay_secs[2] = {
        proc.get_control_value<float>(CONTROL_DELAY_SECS_L),
        proc.get_control_value<float>(CONTROL_DELAY_SECS_R)
    };
    //const bool use_tempo = proc.get_control_value<bool>(CONTROL_USE_TEMPO);

    const float dryf = mix;
    const float wetf = 1.0f - mix;

    //event_reader.new_run(&proc);

    delay_line[0].delay = (size_t)(proc.sample_rate * util::clamp(0.0f, MAX_DELAY_SECS, delay_secs[0]));
    delay_line[1].delay = (size_t)(proc.sample_rate * util::clamp(0.0f, MAX_DELAY_SECS, delay_secs[1]));

    for (size_t i = 0; i < proc.buffer_frame_count * channel_count; i += channel_count) {
        // modx::TrackEvent ev;
        // while (event_reader.read(&ev)) {
        //     if (use_tempo && ev.event_kind == modx::TrackEvent::TEMPO) {
        //         float tempo = ev.tempo;
        //     }
        // }
        
        float sample_l = delay_line[0].read();
        float sample_r = delay_line[1].read();

        output[i] = sample_l * wetf + input[i] * dryf;
        output[i+1] = sample_r * wetf + input[i+1] * dryf;

        delay_line[0].write(input[i] + sample_l * feedback);
        delay_line[1].write(input[i+1] + sample_r * feedback);
    }

    //proc.
}

void DelayModule::ui() {
    bool delay_sync = get_control_value<bool>(CONTROL_DELAY_SYNC);

    // Feedback / Mix //
    ui_knob("Fdb", CONTROL_FEEDBACK, 0.0f, 1.0f);
    ImGui::SameLine();
    ui_knob("W/D", CONTROL_MIX, 0.0f, 1.0f);

    // Sync checkbox //
    ImGui::Text("Sync");
    ImGui::SameLine();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2());
    {
        if (ImGui::Checkbox("##Sync", &delay_sync))
            set_control_value<bool>(CONTROL_DELAY_SYNC, delay_sync);
    }
    ImGui::PopStyleVar();

    // Time knobs //
    if (ui_knob("DlyL", CONTROL_DELAY_SECS_L, 0.0f, MAX_DELAY_SECS))
        if (delay_sync) set_control_value<float>(CONTROL_DELAY_SECS_R, get_control_value<float>(CONTROL_DELAY_SECS_L));
    
    ImGui::SameLine();

    if (ui_knob("DlyR", CONTROL_DELAY_SECS_R, 0.0f, MAX_DELAY_SECS))
        if (delay_sync) set_control_value<float>(CONTROL_DELAY_SECS_L, get_control_value<float>(CONTROL_DELAY_SECS_R));
    
    ImGui::SameLine();
}