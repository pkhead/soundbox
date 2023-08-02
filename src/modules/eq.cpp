#include <imgui.h>
#include "eq.h"
#include "../sys.h"

using namespace audiomod;

EQModule::EQModule(DestinationModule& dest)
:   ModuleBase(dest, true),
    queue(sizeof(module_state), 8)
{
    id = "effect.eq";
    name = "Equalizer";
    
    // low pass
    ui_state.frequency[0] = dest.sample_rate / 2.5f;
    ui_state.resonance[0] = 1.0f;

    // high pass
    ui_state.frequency[1] = 2.0f;
    ui_state.resonance[1] = 1.0f;

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
        module_state* state = (module_state*) handle.data();
        process_state = *state;
        sent_state.clear();
    }

    module_state& state = process_state;
    filter[0].low_pass(sample_rate, state.frequency[0], state.resonance[0]);
    filter[1].high_pass(sample_rate, state.frequency[1], state.resonance[1]);

    float peak_freq[NUM_PEAKS];
    float peak_reso[NUM_PEAKS];
    float peak_enable[NUM_PEAKS];

    for (int i = 0; i < NUM_PEAKS; i++)
    {
        peak_freq[i] = state.peak_frequency[i];
        peak_reso[i] = state.peak_resonance[i];
        peak_enable[i] = state.peak_enabled[i];

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

        float freq = ui_state.frequency[i];
        float reso = ui_state.resonance[i];

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
            &reso, -20.0f, 20.0f,
            "R"
        );

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