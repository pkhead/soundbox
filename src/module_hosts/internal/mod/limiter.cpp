#include "limiter.hpp"
#include "audio_engine/audio_engine.hpp"
#include "dsp.hpp"
#include "imgui.h"
#include "imguiext/imgui-knobs.h"
#include "module_hosts/modules.hpp"
#include "util.hpp"
#include "widgets.hpp"

using namespace hosts::internal;

LimiterModule::LimiterModule(modules::ModuleCreator &create) :
    modx::ModuleBase(create),
    msg_queue(64),
    vu_in(create.engine.sample_rate(), 1.0f),
    vu_out(create.engine.sample_rate(), 1.0f)
{
    cur_limit[0] = 0.0f;
    cur_limit[1] = 0.0f;

    cur_analysis.in_level = 0.0f;
    cur_analysis.out_level = 0.0f;
    cur_analysis.in_peak = 0.0f;
    cur_analysis.out_peak = 0.0f;

    create.add_audio_input(2);
    create.add_audio_output(2);

    create.add_control(CONTROL_INPUT_GAIN, "Input Gain", 0.0f);
    create.add_control(CONTROL_OUTPUT_GAIN, "Output Gain", 0.0f);
    create.add_control(CONTROL_THRESHOLD, "Threshold", -0.5f);
    create.add_control(CONTROL_ATTACK, "Attack", 10.0f);
    create.add_control(CONTROL_DECAY, "Decay", 500.0f);

    create.idle = idle_proc;
}

void LimiterModule::process(modules::ModuleProcessor &proc) {
    constexpr int channel_count = 2;
    const unsigned int sample_rate = proc.sample_rate;

    float *const input = proc.audio_input(0);
    float *const output = proc.audio_output(0);

    const float in_factor = dsp::db_to_mult( proc.get_control_value<float>(CONTROL_INPUT_GAIN) );
    const float out_factor = dsp::db_to_mult( proc.get_control_value<float>(CONTROL_OUTPUT_GAIN) );
    const float threshold = dsp::db_to_mult( proc.get_control_value<float>(CONTROL_THRESHOLD) );

    const float attack = proc.get_control_value<float>(CONTROL_ATTACK);
    const float decay = proc.get_control_value<float>(CONTROL_DECAY);

    float a = powf(0.01f, 1.0f / (attack * sample_rate * 0.001f));
    float r = powf(0.01f, 1.0f / (decay * sample_rate * 0.001f));

    for (size_t i = 0; i < proc.buffer_frame_count * channel_count; i += channel_count) {
        float samples[2];
        samples[0] = input[i] * in_factor;
        samples[1] = input[i+1] * in_factor;

        vu_in.write(util::max(samples[0], samples[1]));
        for (unsigned int c = 0; c < channel_count; c++) {

            float v = fabsf(samples[c]);

            // move the limit towards the amplitude of the current sample
            float& limit = cur_limit[c];
            if (v > limit)
                limit = a * (limit - v) + v;
            else
                limit = r * (limit - v) + v;

            // if limit surpasses the threshold, perform limiting
            if (limit > threshold)
                samples[c] = (samples[c] / limit) * threshold;

            samples[c] *= out_factor; // output gain control
        }
        vu_out.write(util::max(samples[0], samples[1]));

        output[i] = samples[0];
        output[i+1] = samples[1];
    }

    vu_in.update(proc.buffer_frame_count);
    vu_out.update(proc.buffer_frame_count);

    // send vu meter data to ui thread
    MeterData data = {
        vu_in.get_level(),
        vu_out.get_level(),
        vu_in.get_peak(),
        vu_out.get_peak()
    };
    msg_queue.write(&data, 1);
}

void LimiterModule::idle_proc(modules::AudioEngine &engine, modules::ModuleID id, void *userdata) {
    LimiterModule *self = (LimiterModule*) userdata;
    
    // read latest message in queue
    while (self->msg_queue.read(&self->cur_analysis, 1)) {};
}

#define UI_KNOB(label, ctl, min, max, item_fmt)\
    ui_knob(label, ctl, min, max, "%.3f", ImGuiKnobFlags_NoInput);\
    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) { fmt = item_fmt; active_ctl = ctl; }

void LimiterModule::ui() {
    // draw vu meters
    ImGui::BeginGroup();
    ImGui::Text("In");
    ImGui::Text("Out");
    ImGui::EndGroup();

    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::PushItemWidth(ImGui::GetFontSize() * 8.0f);
    widgets::horiz_vu_meter(cur_analysis.in_level, cur_analysis.in_peak, 1.0f);
    widgets::horiz_vu_meter(cur_analysis.out_level, cur_analysis.out_peak, 1.0f);
    ImGui::PopItemWidth();
    ImGui::EndGroup();

    const char *fmt = "%.3f";
    ControlIndex active_ctl = (ControlIndex) -1;

    UI_KNOB("In", CONTROL_INPUT_GAIN, -20.0f, 20.0f, "%.3f dB");
    ImGui::SameLine();
    UI_KNOB("Out", CONTROL_OUTPUT_GAIN, -20.0f, 20.0f, "%.3f dB");

    UI_KNOB("Atk", CONTROL_ATTACK, 1.0f, 1000.0f, "%.3f ms");
    ImGui::SameLine();
    UI_KNOB("Dky", CONTROL_DECAY, 1.0f, 1000.0f, "%.3f ms");
    ImGui::SameLine();
    UI_KNOB("Lim", CONTROL_THRESHOLD, -20.0f, 0.0f, "%.3f dB");

    if ((int)active_ctl != -1) {
        ImGui::TextDisabled(fmt, get_control_value<float>(active_ctl));
    } else {
        ImGui::NewLine();
    }
}