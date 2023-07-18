#include <cstdint>
#include <cmath>
#include <imgui.h>
#include "compressor.h"
#include "../sys.h"
#include "../util.h"

using namespace audiomod;

CompressorModule::CompressorModule(DestinationModule& dest) : ModuleBase(dest, true) {
    id = "effect.compressor";
    name = "Compressor";

    // initialize states
    module_state* states[2] = { &process_state, &ui_state };
    
    for (int i = 0; i < 2; i++) {
        module_state* state = states[i];
        state->input_gain = 0.0f;
        state->output_gain = 0.0f;
        state->attack = 100.0f;
        state->decay = 500.0f;
        state->threshold = -0.5f;
        state->ratio = 1.0f;
    }

    in_volume[0] = 0.0f;
    in_volume[1] = 0.0f;
    out_volume[0] = 0.0f;
    out_volume[1] = 0.0f;
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

void CompressorModule::save_state(std::ostream& ostream)
{
    push_bytes<uint8_t>(ostream, 0); // version
    
    // lock.lock();
    // float input_gain = _input_gain;
    // float output_gain = _output_gain;
    // float cutoff = _cutoff;
    // float decay = _decay;
    // float attack = _attack;
    // float ratio = _ratio;
    // lock.unlock();

    // push_bytes<float>(ostream, input_gain);
    // push_bytes<float>(ostream, output_gain);
    // push_bytes<float>(ostream, cutoff);
    // push_bytes<float>(ostream, decay);
    // push_bytes<float>(ostream, attack);
    // push_bytes<float>(ostream, ratio);
}

bool CompressorModule::load_state(std::istream& istream, size_t size)
{
    uint8_t version = pull_bytesr<uint8_t>(istream);
    if (version != 0) return false;

    // float input_gain = pull_bytesr<float>(istream);
    // float output_gain = pull_bytesr<float>(istream);
    // float cutoff = pull_bytesr<float>(istream);
    // float decay = pull_bytesr<float>(istream);
    // float attack = pull_bytesr<float>(istream);
    // float ratio = pull_bytesr<float>(istream);

    // lock.lock();
    // _input_gain = input_gain;
    // _output_gain = output_gain;
    // _cutoff = cutoff;
    // _decay = decay;
    // _attack = attack;
    // _ratio = ratio;
    // lock.unlock();
    
    return true;
}

void CompressorModule::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) {
    process_state_lock.lock();
    module_state state = process_state;
    process_state_lock.unlock();

    float in_factor = db_to_mult(state.input_gain);
    float out_factor = db_to_mult(state.output_gain);
    float threshold = db_to_mult(state.threshold);

    float a = powf(0.01f, 1.0f / (state.attack * _dest.sample_rate * 0.001f));
    float r = powf(0.01f, 1.0f / (state.decay * _dest.sample_rate * 0.001f));
    
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

            float vabs = fabsf(output[i+c]);

            float target = threshold * powf(vabs / threshold, 1.0f - 1.0f / state.ratio);

            // move the limit towards the amplitude of the current sample,
            // modified by the ratio
            float& limit = _limit[c];

            if (target > limit)
                limit = a * (limit - target) + target;
            else
                limit = r * (limit - target) + target;

            // if limit surpasses the threshold, perform limiting
            if (limit > threshold)
                output[i+c] = (output[i+c] / limit) * threshold;

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

    process_state_lock.lock();
    memcpy(in_volume, inv, 2 * sizeof(float));
    memcpy(out_volume, outv, 2 * sizeof(float));
    process_state_lock.unlock();
}

void CompressorModule::_interface_proc() {
    ImGuiStyle& style = ImGui::GetStyle();
    float in_vol[2], out_vol[2];

    process_state_lock.lock();
    memcpy(in_vol, in_volume, 2 * sizeof(float));
    memcpy(out_vol, out_volume, 2 * sizeof(float));
    process_state_lock.unlock();

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
    float bar_height = ImGui::GetFrameHeight() / 2.0f - style.ItemSpacing.y / 2.0f;
    
    // in volume output
    for (int i = 0; i < 2; i++) {
        ImGui::ProgressBar(in_vol[i], ImVec2(width, bar_height), "");
    }

    // out volume output
    for (int i = 0; i < 2; i++) {
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImGui::ProgressBar(out_vol[i], ImVec2(width, bar_height), "");
    }

    // threshold sliders
    ImGui::SliderFloat("##threshold", &ui_state.threshold, -20.0f, 0, "%.3f dB");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) ui_state.threshold = -10.0f;
    
    // cutoff sliders
    ImGui::SliderFloat("##ratio", &ui_state.ratio, 1.0f, 10.0f, "%.3f dB:1");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) ui_state.ratio = 1.0f;

    // attack
    ImGui::SliderFloat("##attack", &ui_state.attack, 1.0f, 1000.0f, "%.3f ms");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) ui_state.attack = 10.0f;
    
    // decay
    ImGui::SliderFloat("##decay", &ui_state.decay, 1.0f, 1000.0f, "%.3f ms");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) ui_state.decay = 500.0f;
    
    // input gain
    ImGui::SliderFloat("##input_gain", &ui_state.input_gain, -20.0f, 20.0f, "%.3f dB");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) ui_state.input_gain = 0.0f;
    
    // output gain
    ImGui::SliderFloat("##output_gain", &ui_state.output_gain, -20.0f, 20.0f, "%.3f dB");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) ui_state.output_gain = 0.0f;

    ImGui::EndGroup();
    ImGui::PopItemWidth();
    
    process_state_lock.lock();
    process_state = ui_state;
    process_state_lock.unlock();
}
