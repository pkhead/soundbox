#include <iostream>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <imgui.h>
#include "../audio.h"
#include "modules.h"
#include "../sys.h"

using namespace audiomod;

GainModule::GainModule(Song* song) : ModuleBase(song, true) {
    id = "effects.gain";
    name = "Gain";
}

size_t GainModule::save_state(void** output) const
{
    float* out = new float;
    *out = swap_little_endian(gain);
    *output = out;
    return sizeof(float);
}

bool GainModule::load_state(void* state, size_t size)
{
    if (size != sizeof(float)) return false;

    gain = swap_little_endian(*((float*)state));
    return true;
}

void GainModule::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) {
    float factor = powf(10.0f, gain / 10.0f);
    
    for (size_t i = 0; i < buffer_size; i += channel_count) {
        output[i] = 0.0f;
        output[i+1] = 0.0f;

        for (size_t k = 0; k < num_inputs; k++)
        {
            output[i] += inputs[k][i] * factor;
            output[i+1] += inputs[k][i+1] * factor;
        }
    }
}

void GainModule::_interface_proc() {
    ImGui::SetNextItemWidth(200.0f);
    ImGui::SliderFloat("###gain", &gain, -50.0f, 50.0f, "%.3f dB");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) gain = 0.0f;
}