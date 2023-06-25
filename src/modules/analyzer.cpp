// TODO:	shift graph to closest zero crossing to the middle
//			this may require storing audio data in some sort of ring buffer

#include <imgui.h>
#include "analyzer.h"
using namespace audiomod;

AnalyzerModule::AnalyzerModule(DestinationModule& dest) :
    ModuleBase(true),
    ring_buffer(dest.sample_rate * 0.5) // hold 0.5 seconds of audio
{
    id = "effects.analyzer";
    name = "Analyzer";

    buf = new float[frames_per_window * dest.channel_count];
    window_left[0] = new float[frames_per_window];
    window_right[0] = new float[frames_per_window];
    window_left[1] = new float[frames_per_window];
    window_right[1] = new float[frames_per_window];
    ready = false;
}

AnalyzerModule::~AnalyzerModule() {
    ready = false;
    delete[] buf;
    delete[] window_left[0];
    delete[] window_left[1];
    delete[] window_right[0];
    delete[] window_right[1];
}

void AnalyzerModule::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) {
    for (size_t i = 0; i < buffer_size; i += channel_count) {
        output[i] = 0;
        output[i + 1] = 0;

        for (size_t k = 0; k < num_inputs; k++) {
            output[i] += inputs[k][i];
            output[i + 1] += inputs[k][i + 1];
        }
    }

    ring_buffer.write(output, buffer_size);
    size_t samples_per_window = frames_per_window * 2;

    // write to window
    if (!in_use && ring_buffer.queued() > samples_per_window)
    {
        int buf_idx = 1 - window_front;
        
        float* buf_left = window_left[buf_idx];
        float* buf_right = window_right[buf_idx];

        size_t num_read = ring_buffer.read(buf, samples_per_window);
        assert(num_read == samples_per_window);
        
        size_t j = 0;
        for (size_t i = 0; i < frames_per_window * channel_count; i += channel_count) {
            buf_left[j] = buf[i];
            buf_right[j] = buf[i + 1];
            j++;
        }

        if (!in_use) window_front = buf_idx;
    }

    ready = true;
}

void AnalyzerModule::_interface_proc() {
    // use placeholder if audio process isn't ready to show analysis
    float placeholder[2] = { 0.0f, 0.0f };
    bool ready = this->ready;
    in_use = true;

    ImGui::PlotLines(
        "###left_samples", 
        ready ? window_left[window_front] : placeholder,
        ready ? frames_per_window : 2,
        0,
        "L",
        -range,
        range,
        ImVec2(ImGui::GetTextLineHeight() * 15.0f, ImGui::GetTextLineHeight() * 10.0f)
    );

    ImGui::SameLine();
    ImGui::PlotLines(
        "###right_samples", 
        ready ? window_right[window_front] : placeholder,
        ready ? frames_per_window : 2,
        0,
        "R",
        -range,
        range,
        ImVec2(ImGui::GetTextLineHeight() * 15.0f, ImGui::GetTextLineHeight() * 10.0f)
    );
    
    ImGui::SameLine();
    ImGui::VSliderFloat("###range", ImVec2(ImGui::GetTextLineHeight() * 1.5f, ImGui::GetTextLineHeight() * 10.0f), &range, 0.1f, 1.0f, "");
    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
        ImGui::SetTooltip("%.3f", range);
    }

    in_use = false;
}