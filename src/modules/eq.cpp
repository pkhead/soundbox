#include <imgui.h>
#include "eq.h"

using namespace audiomod;

EQModule::EQModule(DestinationModule& dest)
:   ModuleBase(dest, true),
    filter(),
    frequency(dest.sample_rate / 2.5f),
    resonance(1.0f)
{
    id = "effect.eq";
    name = "Equalizer";
}

void EQModule::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) {
    filter.low_pass(sample_rate, frequency, resonance);

    for (int i = 0; i < buffer_size; i += 2)
    {
        float frame_input[2] = { 0.0f, 0.0f };

        for (int j = 0; j < num_inputs; j++)
        {
            frame_input[0] += inputs[j][i];
            frame_input[1] += inputs[j][i + 1];
        }

        filter.process(frame_input, output + i);
    }
}

void EQModule::_interface_proc()
{
    float freq = frequency;
    float reso = resonance;

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Frequency");
    ImGui::SameLine();
    ImGui::SliderFloat(
        "##frequency", &freq, 20.0f, _dest.sample_rate / 2.5f, "%.0f Hz",
        ImGuiSliderFlags_NoRoundToFormat | ImGuiSliderFlags_Logarithmic
    );

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Resonance");
    ImGui::SameLine();
    ImGui::SliderFloat(
        "##resonance", &reso, 1.0f, 50.0f, "%.3f",
        ImGuiSliderFlags_NoRoundToFormat
    );

    frequency = freq;
    resonance = reso;
}

void EQModule::save_state(std::ostream& ostream) const {}
bool EQModule::load_state(std::istream&, size_t size) {
    return true;
}