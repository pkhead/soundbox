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
    _attack = 10.0f;
    _decay = 500.0f;
    _limit[0] = 0.0f;
    _limit[1] = 0.0f;

    for (int i = 0; i < BUFFER_SIZE; i++) {
        _in_buf[0][i] = 0;
        _in_buf[1][i] = 0;
        _out_buf[0][i] = 0;
        _out_buf[1][i] = 0;
        _buf_i[0] = 0;
        _buf_i[1] = 0;
    }
}

void LimiterModule::save_state(std::ostream& ostream)
{
    push_bytes<uint8_t>(ostream, 0); // version
    
    lock.lock();
    float input_gain = _input_gain;
    float output_gain = _output_gain;
    float cutoff = _cutoff;
    float decay = _decay;
    float attack = _attack;
    lock.unlock();

    push_bytes<float>(ostream, input_gain);
    push_bytes<float>(ostream, output_gain);
    push_bytes<float>(ostream, cutoff);
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
    float decay = pull_bytesr<float>(istream);
    float attack = pull_bytesr<float>(istream);

    lock.lock();
    _input_gain = input_gain;
    _output_gain = output_gain;
    _cutoff = cutoff;
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
    float decay = _decay;
    float attack = _attack;
    lock.unlock();

    float in_factor = db_to_mult(input_gain);
    float out_factor = db_to_mult(output_gain);

    float a = powf(0.01f, 1.0f / (attack * _dest.sample_rate * 0.001f));
    float r = powf(0.01f, 1.0f / (decay * _dest.sample_rate * 0.001f));
    
    for (size_t i = 0; i < buffer_size; i += channel_count) {
        // receive inputs
        output[i] = 0.0f;
        output[i+1] = 0.0f;

        for (size_t k = 0; k < num_inputs; k++)
        {
            output[i] += inputs[k][i] * in_factor;
            output[i+1] += inputs[k][i+1] * in_factor;
        }

        for (int c = 0; c < 2; c++)
        {    
            // write input amplitude to buffer for analytics
            _in_buf[c][_buf_i[c]] = output[i+c];

            float v = fabsf(output[i+c]);

            // move the limit towards the amplitude of the current sample
            float& limit = _limit[c];
            if (v > limit)
                limit = a * (limit - v) + v;
            else
                limit = r * (limit - v) + v;

            // if limit surpasses the threshold, perform limiting
            if (limit > cutoff) 
                output[i+c] = (output[i+c] / limit) * cutoff;

            output[i+c] *= out_factor; // output gain control

            // write output amplitude to buffer
            _out_buf[c][_buf_i[c]] = output[i+c];
            _buf_i[c] = (_buf_i[c] + 1) % BUFFER_SIZE; // circular buffer
        }
    }

    // volume analytics
    float inv[2] = { 0.0f, 0.0f };
    float outv[2] = { 0.0f, 0.0f };

    for (int i = 0; i < BUFFER_SIZE; i++)
    {
        for (int c = 0; c < 2; c++)
        {
            if (_in_buf[c][i] > inv[c])
                inv[c] = _in_buf[c][i];

            if (_out_buf[c][i] > outv[c])
                outv[c] = _out_buf[c][i];
        }
    }

    float _limit_out_buf[2];
    _limit_out_buf[0] = max(_limit[0], cutoff);
    _limit_out_buf[1] = max(_limit[1], cutoff);

    lock.lock();
    memcpy(_in_volume, inv, 2 * sizeof(float));
    memcpy(_out_volume, outv, 2 * sizeof(float));
    lock.unlock();
}

void LimiterModule::_interface_proc() {
    ImGuiStyle& style = ImGui::GetStyle();
    float in_vol[2], out_vol[2];

    lock.lock();
    float input_gain = _input_gain;
    float output_gain = _output_gain;
    float cutoff = _cutoff;
    float decay = _decay;
    float attack = _attack;
    memcpy(in_vol, _in_volume, 2 * sizeof(float));
    memcpy(out_vol, _out_volume, 2 * sizeof(float));
    lock.unlock();

    float width = ImGui::GetTextLineHeight() * 14.0f;
    ImGui::PushItemWidth(width);

    ImGui::BeginGroup();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("In");
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Out");
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Threshold");
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
    float bar_height = ImGui::GetFrameHeight() / 2.0f - style.ItemSpacing.y / 2.0f;
    
    for (int i = 0; i < 2; i++) {
        ImGui::ProgressBar(in_vol[i], ImVec2(width, bar_height), "");
    }

    for (int i = 0; i < 2; i++) {
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImGui::ProgressBar(out_vol[i], ImVec2(width, bar_height), "");
    }

    ImGui::SliderFloat("##threshold", &cutoff, -20.0f, 0, "%.3f dB");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) cutoff = -1.0f;
    ImGui::SliderFloat("##attack", &attack, 1.0f, 1000.0f, "%.3f ms");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) attack = 10.0f;
    ImGui::SliderFloat("##decay", &decay, 1.0f, 1000.0f, "%.3f ms");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) decay = 500.0f;
    ImGui::SliderFloat("##input_gain", &input_gain, -20.0f, 20.0f, "%.3f dB");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) input_gain = 0.0f;
    ImGui::SliderFloat("##output_gain", &output_gain, -20.0f, 20.0f, "%.3f dB");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) output_gain = 0.0f;

    ImGui::EndGroup();
    ImGui::PopItemWidth();

    lock.lock();
    _input_gain = input_gain;
    _output_gain = output_gain;
    _cutoff = cutoff;
    _decay = decay;
    _attack = attack;
    lock.unlock();
}
