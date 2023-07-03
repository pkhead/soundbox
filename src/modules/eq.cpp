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
}

void EQModule::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) {
    filter[0].low_pass(sample_rate, frequency[0], resonance[0]);
    filter[1].high_pass(sample_rate, frequency[1], resonance[1]);

    for (int i = 0; i < buffer_size; i += 2)
    {
        float frame_input[2] = { 0.0f, 0.0f };

        for (int j = 0; j < num_inputs; j++)
        {
            frame_input[0] += inputs[j][i];
            frame_input[1] += inputs[j][i + 1];
        }

        filter[0].process(frame_input, output + i);
        filter[1].process(output + i, output + i);
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
            "##resonance", &reso, 1.0f, 20.0f, "%.3f",
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
    ImGui::Text("Band-pass");

    static float _test_freq = 0.0f;
    static float _test_reso = 0.0f;
    static bool _test_bool = false;

    ImVec2 vslider_size = ImVec2( ImGui::GetFrameHeight(), ImGui::GetFrameHeightWithSpacing() * 4.0 + ImGui::GetTextLineHeight() );
    
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + style.FramePadding.y);
    for (int i = 0; i < 8; i++)
    {
        ImGui::PushID(i);

        if (i > 0) ImGui::SameLine();

        const float spacing = style.ItemSpacing.y;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, spacing));
        
        ImGui::BeginGroup();

        ImGui::VSliderFloat(
            "##bp-frequency",
            vslider_size,
            &_test_freq, 20.0f, _dest.sample_rate / 2.5f,
            "F", ImGuiSliderFlags_Logarithmic
        );

        if ((ImGui::IsItemHovered() || ImGui::IsItemActive()) && ImGui::BeginTooltip()) {
            ImGui::Text("Frequency: %.3f Hz", _test_freq);
            ImGui::EndTooltip();
        }

        ImGui::SameLine();
        ImGui::VSliderFloat(
            "##bp-resonance",
            vslider_size,
            &_test_reso, 1.0f, 20.0f,
            "R"
        );

        if ((ImGui::IsItemHovered() || ImGui::IsItemActive()) && ImGui::BeginTooltip()) {
            ImGui::Text("Resonance: %.3f dB", _test_reso);
            ImGui::EndTooltip();
        }

        ImGui::PopStyleVar();

        ImGui::Checkbox("##enabled", &_test_bool);

        ImGui::EndGroup();
        ImGui::PopID();
    }
    ImGui::EndGroup();
}

void EQModule::save_state(std::ostream& ostream) const {}
bool EQModule::load_state(std::istream&, size_t size) {
    return true;
}