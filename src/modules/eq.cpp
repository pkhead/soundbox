#include <imgui.h>
#include "eq.h"

using namespace audiomod;

EQModule::EQModule(DestinationModule& dest)
:   ModuleBase(dest, true)
{
    id = "effect.eq";
    name = "Equalizer";

    // low pass
    frequency[0] = dest.sample_rate / 2.5f;
    resonance[0] = 1.0f;

    // high pass
    frequency[1] = 2.0f;
    resonance[1] = 1.0f;

    // peaks
    for (int i = 0; i < NUM_PEAKS; i++)
    {
        peak_frequency[i] = ((float)i / NUM_PEAKS) * dest.sample_rate;
        peak_resonance[i] = 1.0f;
        peak_enabled[i] = false;
    }
}

void EQModule::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) {
    filter[0].low_pass(sample_rate, frequency[0], resonance[0]);
    filter[1].high_pass(sample_rate, frequency[1], resonance[1]);

    float peak_freq[NUM_PEAKS];
    float peak_reso[NUM_PEAKS];
    float peak_enable[NUM_PEAKS];

    for (int i = 0; i < NUM_PEAKS; i++)
    {
        peak_freq[i] = peak_frequency[i];
        peak_reso[i] = peak_resonance[i];
        peak_enable[i] = peak_enabled[i];

        if (peak_enable[i])
            peak_filter[i].peak(_dest.sample_rate, peak_freq[i], peak_reso[i], 0.3f);
    }

    for (int i = 0; i < buffer_size; i += 2)
    {
        output[i] = 0.0f;
        output[i + 1] = 0.0f;

        for (int j = 0; j < num_inputs; j++)
        {
            output[i] += inputs[j][i];
            output[i + 1] += inputs[j][i + 1];
        }

        filter[0].process(output + i, output + i);
        filter[1].process(output + i, output + i);

        for (int j = 0; j < NUM_PEAKS; j++)
        {
            if (peak_enable[j]) {
                peak_filter[j].process(output + i, output + i);
            }
        }
    }
}

void EQModule::_interface_proc()
{
    // TODO: eq display controlled not by sliders, but by
    //       dragging points on a frequency response display
    //       just like in BeepBox

    ImGui::BeginGroup();
    ImGui::AlignTextToFramePadding();
    ImGuiStyle& style = ImGui::GetStyle();

    ImGui::PushItemWidth(ImGui::GetTextLineHeight() * 10.0f);

    for (int i = 0; i < 2; i++)
    {
        ImGui::PushID(i);

        float freq = frequency[i];
        float reso = resonance[i];

        if (i == 0) ImGui::Text("Low-pass");
        if (i == 1) ImGui::Text("High-pass");

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
            "##resonance", &reso, 0.001f, 10.0f, "%.3f",
            ImGuiSliderFlags_NoRoundToFormat
        );

        frequency[i] = freq;
        resonance[i] = reso;

        ImGui::PopID();
    }

    ImGui::PopItemWidth();
    ImGui::EndGroup();
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::Text("Peaks");

    ImVec2 vslider_size = ImVec2( ImGui::GetFrameHeight(), ImGui::GetFrameHeightWithSpacing() * 4.0 + ImGui::GetTextLineHeight() );
    
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + style.FramePadding.y);

    for (int i = 0; i < NUM_PEAKS; i++)
    {
        float freq = peak_frequency[i];
        float reso = peak_resonance[i];
        bool enabled = peak_enabled[i];

        ImGui::PushID(i);

        if (i > 0) ImGui::SameLine();

        const float spacing = style.ItemSpacing.y;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, spacing));
        
        ImGui::BeginGroup();

        ImGui::VSliderFloat(
            "##peak-frequency",
            vslider_size,
            &freq, 20.0f, _dest.sample_rate / 2.5f,
            "F", ImGuiSliderFlags_Logarithmic
        );

        if ((ImGui::IsItemHovered() || ImGui::IsItemActive()) && ImGui::BeginTooltip()) {
            ImGui::Text("Frequency: %.3f Hz", freq);
            ImGui::EndTooltip();
        }

        ImGui::SameLine();
        ImGui::VSliderFloat(
            "##peak-resonance",
            vslider_size,
            &reso, -15.0f, 15.0f,
            "R"
        );

        if ((ImGui::IsItemHovered() || ImGui::IsItemActive()) && ImGui::BeginTooltip()) {
            ImGui::Text("Resonance: %.3f dB", reso);
            ImGui::EndTooltip();
        }

        ImGui::PopStyleVar();

        ImGui::Checkbox("##enabled", &enabled);

        peak_frequency[i] = freq;
        peak_resonance[i] = reso;
        peak_enabled[i] = enabled;

        ImGui::EndGroup();
        ImGui::PopID();
    }
    ImGui::EndGroup();
}

void EQModule::save_state(std::ostream& ostream) const {}
bool EQModule::load_state(std::istream&, size_t size) {
    return true;
}