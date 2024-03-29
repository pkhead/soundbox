#include <imgui.h>
#include <cmath>
#include "eq.h"
#include "../sys.h"

using namespace audiomod;

EQModule::EQModule(ModuleContext& dest)
:   ModuleBase(true), modctx(dest),
    queue(sizeof(module_state), 8)
{
    id = "effect.eq";
    name = "Equalizer";
    
    // low pass
    ui_state.frequency[0] = dest.sample_rate / 2.5f;
    ui_state.resonance[0] = 0.0f;

    // high pass
    ui_state.frequency[1] = 20.0f;
    ui_state.resonance[1] = 0.0f;

    // peaks
    for (int i = 0; i < NUM_PEAKS; i++)
    {
        ui_state.peak_frequency[i] = dest.sample_rate / 4.0f;
        ui_state.peak_resonance[i] = 0.0f;
        ui_state.peak_enabled[i] = false;
    }

    process_state = ui_state;
}

void EQModule::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) {
    // read messages sent from ui thread
    while (true)
    {
        MessageQueue::read_handle_t handle = queue.read();
        if (!handle) break;

        assert(handle.size() == sizeof(module_state));
        module_state state;
        handle.read(&state, sizeof(module_state));
        
        process_state = state;
        sent_state.clear();
    }

    module_state& state = process_state;

    for (int c = 0; c < 2; c++) {
        filter[0][c].low_pass(sample_rate, state.frequency[0], db_to_mult(state.resonance[0]));
        filter[1][c].high_pass(sample_rate, state.frequency[1], db_to_mult(state.resonance[1]));
    }

    float peak_freq[NUM_PEAKS];
    float peak_reso[NUM_PEAKS];
    float peak_enable[NUM_PEAKS];

    for (int i = 0; i < NUM_PEAKS; i++)
    {
        peak_freq[i] = state.peak_frequency[i];
        peak_reso[i] = state.peak_resonance[i];
        peak_enable[i] = state.peak_enabled[i];

        if (peak_enable[i])
            for (int c = 0; c < 2; c++)
                peak_filter[i][c].peak(modctx.sample_rate, peak_freq[i], peak_reso[i], 0.3f);
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

        for (int c = 0; c < 2; c++)
        {
            filter[0][c].process(output + i + c);
            filter[1][c].process(output + i + c);
        }
        for (int j = 0; j < NUM_PEAKS; j++)
        {
            if (peak_enable[j]) {
                peak_filter[j][0].process(output + i);
                peak_filter[j][1].process(output + i + 1);
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

        float freq = ui_state.frequency[i];
        float reso = ui_state.resonance[i];

        if (i == 0) ImGui::Text("Low-pass");
        if (i == 1) ImGui::Text("High-pass");

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Frequency");
        ImGui::SameLine();
        ImGui::SliderFloat(
            "##frequency", &freq, 20.0f, modctx.sample_rate / 2.5f, "%.0f Hz",
            ImGuiSliderFlags_NoRoundToFormat | ImGuiSliderFlags_Logarithmic
        );
        if (ImGui::IsItemClicked(ImGuiMouseButton_Middle))
            freq = i == 0 ? modctx.sample_rate / 2.5f : 20.0f;

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Resonance");
        ImGui::SameLine();
        ImGui::SliderFloat(
            "##resonance", &reso, -10.0f, 10.0f, "%.3f",
            ImGuiSliderFlags_NoRoundToFormat
        );
        if (ImGui::IsItemClicked(ImGuiMouseButton_Middle))
            reso = 0.0f;

        ui_state.frequency[i] = freq;
        ui_state.resonance[i] = reso;

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
        float freq = ui_state.peak_frequency[i];
        float reso = ui_state.peak_resonance[i];
        bool enabled = ui_state.peak_enabled[i];

        ImGui::PushID(i);

        if (i > 0) ImGui::SameLine();

        const float spacing = style.ItemSpacing.y;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, spacing));
        
        ImGui::BeginGroup();

        ImGui::VSliderFloat(
            "##peak-frequency",
            vslider_size,
            &freq, 20.0f, modctx.sample_rate / 2.5f,
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
            &reso, -60.0f, 60.0f,
            "R"
        );
        if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) reso = 0;

        if ((ImGui::IsItemHovered() || ImGui::IsItemActive()) && ImGui::BeginTooltip()) {
            ImGui::Text("Resonance: %.3f dB", reso);
            ImGui::EndTooltip();
        }

        ImGui::PopStyleVar();

        ImGui::Checkbox("##enabled", &enabled);

        ui_state.peak_frequency[i] = freq;
        ui_state.peak_resonance[i] = reso;
        ui_state.peak_enabled[i] = enabled;

        ImGui::EndGroup();
        ImGui::PopID();
    }
    ImGui::EndGroup();

    // TODO: analysis should be done in audio thread
    std::vector<float> response;
    size_t graph_w = modctx.sample_rate / 2.0f;
    response.reserve(graph_w);
    Filter2ndOrder lp_filter = filter[0][0];
    Filter2ndOrder hp_filter = filter[1][0];

    for (float hz = 20; hz < graph_w; hz *= 1.1)
    {
        float m = 1;
        m *= lp_filter.attenuation(hz, modctx.sample_rate);
        m *= hp_filter.attenuation(hz, modctx.sample_rate);

        for (int i = 0; i < NUM_PEAKS; i++)
        {
            if (ui_state.peak_enabled[i]) {
                Filter2ndOrder peak = peak_filter[i][0];
                m *= peak.attenuation(hz, modctx.sample_rate);
            }
        }

        response.push_back(10 * logf(m));
    }

    ImGui::PlotLines(
        "##res",
        response.data(),
        response.size(),
        0,
        nullptr,
        -30.0f,
        30.0f,
        ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 12)
    );

    // send updated module state to audio thread
    if (!sent_state.test_and_set()) {
        if (queue.post(&ui_state, sizeof(ui_state))) {
            dbg("WARNING: LimiterModule process queue is full!\n");
        }
    }

}

// TODO: test this function
void EQModule::save_state(std::ostream& ostream) {
    push_bytes<uint8_t>(ostream, 0); // version

    // lpf/hpf
    push_bytes<float>(ostream, ui_state.frequency[0]);
    push_bytes<float>(ostream, ui_state.frequency[1]);
    push_bytes<float>(ostream, ui_state.resonance[0]);
    push_bytes<float>(ostream, ui_state.resonance[1]);

    // peak filters
    // "enabled" fields are written into a bitfield
    constexpr size_t byte_count = (NUM_PEAKS + 8 - 1) / 8; // NUM_PEAKS / 8, rounded up

    for (size_t i = 0; i < byte_count; i++)
    {
        uint8_t bitfield = 0;

        for (size_t j = 0; j < 8; j++) {
            bitfield |= ui_state.peak_enabled[i*8+j] << j;
        }

        push_bytes<uint8_t>(ostream, bitfield);
    }

    for (int i = 0; i < NUM_PEAKS; i++)
    {
        push_bytes<float>(ostream, ui_state.peak_frequency[i]);
        push_bytes<float>(ostream, ui_state.peak_resonance[i]);
    }
}

bool EQModule::load_state(std::istream& istream, size_t size) {
    // verify version
    if (pull_bytesr<uint8_t>(istream) != 0) return false;

    // lpf/hpf
    ui_state.frequency[0] = pull_bytesr<float>(istream);
    ui_state.frequency[1] = pull_bytesr<float>(istream);
    ui_state.resonance[0] = pull_bytesr<float>(istream);
    ui_state.resonance[1] = pull_bytesr<float>(istream);

    // peak filters
    // "enabled" fields are read from a bitfield
    constexpr size_t byte_count = (NUM_PEAKS + 8 - 1) / 8; // NUM_PEAKS / 8, rounded up

    for (size_t i = 0; i < byte_count; i++)
    {
        uint8_t bitfield = pull_bytesr<uint8_t>(istream);

        for (size_t j = 0; j < 8; j++) {
            ui_state.peak_enabled[i*8+j] = (bitfield >> j) & 1;
        }
    }

    // frequency/resonance
    for (int i = 0; i < NUM_PEAKS; i++)
    {
        ui_state.peak_frequency[i] = pull_bytesr<float>(istream);
        ui_state.peak_resonance[i] = pull_bytesr<float>(istream);
    }

    if (queue.post(&ui_state, sizeof(ui_state))) {
        dbg("WARNING: LimiterModule process queue is full!\n");
        return false;
    }

    return true;
}

#ifdef UNIT_TESTS
#include <catch2/catch_amalgamated.hpp>

/*
TEST_CASE("EQModule serialization", "[modules]")
{
    ModuleContext dest(48000, 2, 1024);
    EQModule mod(dest);

    mod.ui_state.frequency[0] = 1.0f;
    mod.ui_state.frequency[1] = 2.0f;
    mod.ui_state.resonance[0] = 3.0f;
    mod.ui_state.resonance[1] = 4.0f;

    for (int i = 0; i < mod.NUM_PEAKS; i++)
    {
        mod.ui_state.peak_enabled[i] = i % 2 != 0;
        mod.ui_state.peak_frequency[i] = 5.0f * i;
        mod.ui_state.peak_resonance[i] = 4.0f * i;
    }

    std::stringstream stream;
    EQModule::module_state state = mod.ui_state;

    mod.save_state(stream);
    mod.load_state(stream, stream.tellp());
    REQUIRE(memcmp(&mod.ui_state, &state, sizeof(EQModule::module_state)) == 0);
}
*/

#endif
