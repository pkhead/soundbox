// TODO:	shift graph to closest zero crossing to the middle
//			this may require storing audio data in some sort of ring buffer

#include <imgui.h>
#include "../util.h"
#include "analyzer.h"

using namespace audiomod;

AnalyzerModule::AnalyzerModule(DestinationModule& dest) :
    ModuleBase(true),
    ring_buffer(dest.sample_rate * 0.5) // hold 0.5 seconds of audio
{
    id = "effect.analyzer";
    name = "Analyzer";

    size_t arr_size = frames_per_window + window_margin * 2;
    buf = new float[arr_size * dest.channel_count];
    window_left[0] = new float[arr_size];
    window_right[0] = new float[arr_size];
    window_left[1] = new float[arr_size];
    window_right[1] = new float[arr_size];
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
    size_t samples_per_window = (frames_per_window + window_margin * 2) * 2;

    // write to window
    if (!in_use && ring_buffer.queued() > samples_per_window)
    {
        int buf_idx = 1 - window_front;
        
        float* buf_left = window_left[buf_idx];
        float* buf_right = window_right[buf_idx];

        size_t num_read = ring_buffer.read(buf, samples_per_window);
        assert(num_read == samples_per_window);

        size_t j = 0;
        for (size_t i = 0; i < samples_per_window; i += channel_count) {
            buf_left[j] = buf[i];
            buf_right[j] = buf[i + 1];
            j++;
        }

        if (!in_use) window_front = buf_idx;
    }

    ready = true;
}

static int offset_zero_crossing(float* buf, size_t buf_size, size_t border)
{
    // search both left and right side for a zero crossing
    int origin = buf_size / 2;
    float cur[2], prev[2];

    for (int i = 0; i + origin < buf_size - border - 1 && i + origin > border; i++)
    {
        cur[0] = buf[origin - i]; // left side
        prev[0] = buf[origin - i - 1];
        cur[1] = buf[origin + i]; // right side
        prev[1] = buf[origin + i - 1];

        // if there is a rising zero crossing on the left side
        if (is_zero_crossing(prev[0], cur[0]) && cur[0] > prev[0])
            return -i;

        // if there is rising zero crossing on the right side
        else if (is_zero_crossing(prev[1], cur[1]) && cur[1] > prev[1])
            return i;
    }

    return 0;
}

void AnalyzerModule::_interface_proc() {
    // use placeholder if audio process isn't ready to show analysis
    float placeholder[2] = { 0.0f, 0.0f };
    bool ready = this->ready;
    in_use = true;

    ImVec2 graph_size = ImVec2(ImGui::GetTextLineHeight() * 15.0f, ImGui::GetTextLineHeight() * 10.0f);

    if (ready)
    {
        float* left = window_left[window_front];
        float* right = window_right[window_front];

        int offset_left = 0;
        int offset_right = 0;

        if (align) {
            // align display to nearest zero crossing from center
            size_t frames_per_buffer = frames_per_window + window_margin * 2;
            offset_left = offset_zero_crossing(left, frames_per_buffer, window_margin);
            offset_right = offset_zero_crossing(right, frames_per_buffer, window_margin);
        }

        ImGui::PlotLines(
            "###left_samples", 
            left + window_margin + offset_left,
            frames_per_window,
            0,
            "L",
            -range,
            range,
            graph_size
        );

        ImGui::SameLine();
        ImGui::PlotLines(
            "###right_samples", 
            right + window_margin + offset_right,
            frames_per_window,
            0,
            "R",
            -range,
            range,
            graph_size
        );
    }
    else
    {
        // processing thread is not ready to show data,
        // display a placeholder instead
        ImGui::PlotLines(
            "###left_samples", 
            placeholder,
            2,
            0,
            "L",
            -range,
            range,
            graph_size
        );

        ImGui::SameLine();
        ImGui::PlotLines(
            "###right_samples", 
            placeholder,
            2,
            0,
            "R",
            -range,
            range,
            graph_size
        );
    }
    
    ImGui::SameLine();
    ImGui::VSliderFloat(
        "###range",
        ImVec2(ImGui::GetTextLineHeight() * 1.5f, ImGui::GetTextLineHeight() * 10.0f),
        &range,
        0.01f,
        1.0f,
        "",
        ImGuiSliderFlags_Logarithmic);
    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
        ImGui::SetTooltip("%.3f", range);
    }

    in_use = false;

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Align");
    ImGui::SameLine();
    ImGui::Checkbox("###align", &align);
}