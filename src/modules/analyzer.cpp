// TODO:	shift graph to closest zero crossing to the middle
//			this may require storing audio data in some sort of ring buffer

#include <imgui.h>
#include "analyzer.h"
using namespace audiomod;

AnalyzerModule::AnalyzerModule(Song* song) : ModuleBase(song, true)
{
    id = "effects.analyzer";
    name = "Analyzer";
}

AnalyzerModule::~AnalyzerModule() {
    ready = false;

    if (left_channel[0]) delete[] left_channel[0];
    if (right_channel[0]) delete[] right_channel[0];
    if (left_channel[1]) delete[] left_channel[1];
    if (right_channel[1]) delete[] right_channel[1];
}

void AnalyzerModule::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) {
    // TODO instead of reallocating buffers here, make a buffer_realloc event/signal
    if (buffer_size != this->buf_size) {
        // tell renderer process not to render
        ready = false;

        if (left_channel[0]) delete[] left_channel[0];
        if (right_channel[0]) delete[] right_channel[0];
        if (left_channel[1]) delete[] left_channel[1];
        if (right_channel[1]) delete[] right_channel[1];

        buf_size = buffer_size / 2;
        left_channel[0] = new float[buf_size];
        right_channel[0] = new float[buf_size];
        left_channel[1] = new float[buf_size];
        right_channel[1] = new float[buf_size];
    }

    int buf_idx;
    // if in use, do not swap buffers
    if (in_use)		buf_idx = front_buf;
    else			buf_idx = 1 - front_buf;
    
    float* buf_left = left_channel[buf_idx];
    float* buf_right = right_channel[buf_idx];

    size_t j = 0;
    for (size_t i = 0; i < buffer_size; i += channel_count) {
        buf_left[j] = 0;
        buf_right[j] = 0;
        output[i] = 0;
        output[i + 1] = 0;

        for (size_t k = 0; k < num_inputs; k++) {
            buf_left[j] += inputs[k][i];
            buf_right[j] += inputs[k][i + 1];

            output[i] += inputs[k][i];
            output[i + 1] += inputs[k][i + 1];
        }

        j++;
    }

    front_buf = buf_idx;
    if (!ready) ready = true;
}

void AnalyzerModule::_interface_proc() {
    // use placeholder if audio process isn't ready to show analysis
    float placeholder[2] = { 0.0f, 0.0f };
    bool ready = this->ready;
    in_use = true;

    ImGui::PlotLines(
        "###left_samples", 
        ready ? left_channel[front_buf] : placeholder,
        ready ? buf_size : 2,
        0,
        "L",
        -range,
        range,
        ImVec2(ImGui::GetTextLineHeight() * 15.0f, ImGui::GetTextLineHeight() * 10.0f)
    );

    ImGui::SameLine();
    ImGui::PlotLines(
        "###right_samples", 
        ready ? right_channel[front_buf] : placeholder,
        ready ? buf_size : 2,
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