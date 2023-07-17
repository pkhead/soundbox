#include <cstdint>
#include <cmath>
#include <imgui.h>
#include "limiter.h"
#include "../sys.h"
#include "../util.h"

using namespace audiomod;

LimiterModule::LimiterModule(DestinationModule& dest) : ModuleBase(dest, true) {
    id = "effect.limiter";
    name = "Limiter";

    _input_gain = 0.0f;
    _output_gain = 0.0f;
    _cutoff = -1.0f;
    _ratio = 10.0f;
    _attack = 10.0f;
    _decay = 500.0f;
    _envelope[0] = 0.0f;
    _envelope[1] = 0.0f;
}

void LimiterModule::save_state(std::ostream& ostream)
{
    push_bytes<uint8_t>(ostream, 0); // version
    
    lock.lock();
    float input_gain = _input_gain;
    float output_gain = _output_gain;
    float cutoff = db_to_mult(_cutoff);
    float ratio = _ratio;
    float decay = _decay;
    float attack = _attack;
    lock.unlock();

    push_bytes<float>(ostream, input_gain);
    push_bytes<float>(ostream, output_gain);
    push_bytes<float>(ostream, cutoff);
    push_bytes<float>(ostream, ratio);
    push_bytes<float>(ostream, decay);
    push_bytes<float>(ostream, attack);
}

bool LimiterModule::load_state(std::istream& istream, size_t size)
{
    uint8_t version = pull_bytesr<uint8_t>(istream);
    if (version != 0) return false;

    float input_gain = pull_bytesr<float>(istream);
    float output_gain = pull_bytesr<float>(istream);
    float cutoff = pull_bytesr<float>(istream);
    float ratio = pull_bytesr<float>(istream);
    float decay = pull_bytesr<float>(istream);
    float attack = pull_bytesr<float>(istream);

    lock.lock();
    _input_gain = input_gain;
    _output_gain = output_gain;
    _cutoff = cutoff;
    _ratio = ratio;
    _decay = decay;
    _attack = attack;
    lock.unlock();
    
    return true;
}

void LimiterModule::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) {
    lock.lock();
    float input_gain = _input_gain;
    float output_gain = _output_gain;
    float cutoff = db_to_mult(_cutoff);
    float ratio = _ratio;
    float decay = _decay;
    float attack = _attack;
    lock.unlock();

    float in_factor = db_to_mult(input_gain);
    float out_factor = db_to_mult(output_gain);

    //float a = powf(0.01f, 1.0f / (attack * _dest.sample_rate * 0.001f));
    //float r = powf(0.01f, 1.0f / (decay * _dest.sample_rate * 0.001f));
    
    for (size_t i = 0; i < buffer_size; i += channel_count) {
        // receive inputs
        output[i] = 0.0f;
        output[i+1] = 0.0f;

        for (size_t k = 0; k < num_inputs; k++)
        {
            output[i] += inputs[k][i] * in_factor;
            output[i+1] += inputs[k][i+1] * in_factor;
        }

        // limiter stuff
        for (int c = 0; c < 2; c++)
        {
            float v = fabsf(output[i+c]);

            if (v > cutoff) {
                _envelope[c] = min(_envelope[c] + (1000.0f / attack) / _dest.sample_rate, 1.0f);
            } else {
                _envelope[c] = max(_envelope[c] - (1000.0f / decay) / _dest.sample_rate, 0.0f);
            }

            float limited = cutoff * powf(v / cutoff, 1.0f / powf(ratio, _envelope[c]));

            if (v == 0.0f)
                output[i+c] = 0.0f;
            else
                output[i+c] *= limited / v * out_factor;
        }
    }
}

void LimiterModule::_interface_proc() {
    lock.lock();
    float input_gain = _input_gain;
    float output_gain = _output_gain;
    float cutoff = _cutoff;
    float ratio = _ratio;
    float decay = _decay;
    float attack = _attack;
    lock.unlock();

    ImGui::SetNextItemWidth(50.0f);

    ImGui::BeginGroup();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Threshold");
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Ratio");
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Attack");
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Decay");
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Input Gain");
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Output Gain");
    ImGui::EndGroup();

    ImGui::SameLine();

    ImGui::BeginGroup();
    ImGui::SliderFloat("##threshold", &cutoff, -20.0f, 0, "%.3f dB");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) cutoff = -1.0f;
    ImGui::SliderFloat("##ratio", &ratio, 1.0f, 50.0f, "%.3f:1");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) ratio = 10.0f;
    ImGui::SliderFloat("##attack", &attack, 1.0f, 1000.0f, "%.3f ms");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) attack = 10.0f;
    ImGui::SliderFloat("##decay", &decay, 1.0f, 1000.0f, "%.3f ms");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) decay = 500.0f;
    ImGui::SliderFloat("##input_gain", &input_gain, -20.0f, 20.0f, "%.3f dB");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) input_gain = 0.0f;
    ImGui::SliderFloat("##output_gain", &output_gain, -20.0f, 20.0f, "%.3f dB");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) output_gain = 0.0f;

    ImGui::EndGroup();

    lock.lock();
    _input_gain = input_gain;
    _output_gain = output_gain;
    _cutoff = cutoff;
    _ratio = ratio;
    _decay = decay;
    _attack = attack;
    lock.unlock();
    //
}