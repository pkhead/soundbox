#include <iostream>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <imgui.h>
#include "../audio.h"
#include "../sys.h"

using namespace audiomod;

GainModule::GainModule() : ModuleBase(true) {
    id = "effects.gain";
    name = "Gain";
}

void GainModule::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) {
    for (size_t i = 0; i < buffer_size; i += channel_count) {
        output[i] = inputs[0][i];
        output[i+1] = inputs[0][i+1];
    }
}

void GainModule::_interface_proc() {
    ImGui::SetNextItemWidth(200.0f);
    ImGui::SliderFloat("###gain", &gain, -10.0f, 10.0f);
}