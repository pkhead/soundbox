#include <cstdint>
#include <cmath>
#include <imgui.h>
#include "limiter.h"
#include "../sys.h"
#include "../util.h"

using namespace audiomod;

LimiterModule::LimiterModule(DestinationModule& dest)
:   ModuleBase(dest, true),
    process_queue(sizeof(message_t), 8),
    ui_queue(sizeof(message_t), 8)
{
    id = "effect.limiter";
    name = "Limiter";
    
    // initialize states
    module_state* states[2] = { &process_state, &ui_state };
    analytics_t* analytics[2] = { &ui_analytics, &process_analytics };

    for (int i = 0; i < 2; i++) {
        module_state* state = states[i];
        state->input_gain = 0.0f;
        state->output_gain = 0.0f;
        state->attack = 10.0f;
        state->decay = 500.0f;
        state->threshold = -0.5f;

        analytics_t* a = analytics[i];
        a->in_volume[0] = 0.0f;
        a->in_volume[1] = 0.0f;
        a->out_volume[0] = 0.0f;
        a->out_volume[1] = 0.0f;
    }

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

    push_bytes<float>(ostream, ui_state.input_gain);
    push_bytes<float>(ostream, ui_state.output_gain);
    push_bytes<float>(ostream, ui_state.threshold);
    push_bytes<float>(ostream, ui_state.decay);
    push_bytes<float>(ostream, ui_state.attack);
}

bool LimiterModule::load_state(std::istream& istream, size_t size)
{
    uint8_t version = pull_bytesr<uint8_t>(istream);
    if (version != 0) return false;

    float input_gain = pull_bytesr<float>(istream);
    float output_gain = pull_bytesr<float>(istream);
    float threshold = pull_bytesr<float>(istream);
    float decay = pull_bytesr<float>(istream);
    float attack = pull_bytesr<float>(istream);

    ui_state.input_gain = input_gain;
    ui_state.output_gain = output_gain;
    ui_state.threshold = threshold;
    ui_state.decay = decay;
    ui_state.attack = attack;

    // send state to processing
    message_t new_msg;

    new_msg.type = message_t::ModuleState;
    new_msg.mod_state = ui_state;
    if (process_queue.post(&new_msg, sizeof(new_msg))) {
        dbg("WARNING: LimiterModule process queue is full!\n");
    }

    return true;
}

void LimiterModule::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) {
    // read messages sent from ui thread
    while (true)
    {
        MessageQueue::read_handle_t handle = process_queue.read();
        if (!handle) break;

        assert(handle.size() == sizeof(message_t));
        message_t msg;
        handle.read(&msg, sizeof(msg));

        // update module state
        if (msg.type == message_t::ModuleState) {
            process_state = msg.mod_state;
        }

        // send analytics to ui thread
        else if (msg.type == message_t::RequestAnalytics) {
            message_t new_msg;
            new_msg.type = message_t::ReceiveAnalytics;
            new_msg.analytics = process_analytics;

            ui_queue.post(&new_msg, sizeof(new_msg));
        }
    }

    module_state& state = process_state;

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

            float v = fabsf(output[i+c]);

            // move the limit towards the amplitude of the current sample
            float& limit = _limit[c];
            if (v > limit)
                limit = a * (limit - v) + v;
            else
                limit = r * (limit - v) + v;

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

    float _limit_out_buf[2];
    _limit_out_buf[0] = max(_limit[0], state.threshold);
    _limit_out_buf[1] = max(_limit[1], state.threshold);

    memcpy(process_analytics.in_volume, inv, 2 * sizeof(float));
    memcpy(process_analytics.out_volume, outv, 2 * sizeof(float));
}

void LimiterModule::_interface_proc() {
    ImGuiStyle& style = ImGui::GetStyle();
    float in_vol[2], out_vol[2];
    
    // read messages sent from process thread
    while (true)
    {
        MessageQueue::read_handle_t handle = ui_queue.read();
        if (!handle) break;

        assert(handle.size() == sizeof(message_t));
        message_t msg;
        handle.read(&msg, sizeof(msg));

        // update analytics state
        if (msg.type == message_t::ReceiveAnalytics) {
            ui_analytics = msg.analytics;
            waiting = false;
        }
    }

    memcpy(in_vol, ui_analytics.in_volume, 2 * sizeof(float));
    memcpy(out_vol, ui_analytics.out_volume, 2 * sizeof(float));

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

    ImGui::SliderFloat("##threshold", &ui_state.threshold, -20.0f, 0, "%.3f dB");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) ui_state.threshold = -1.0f;
    ImGui::SliderFloat("##attack", &ui_state.attack, 1.0f, 1000.0f, "%.3f ms");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) ui_state.attack = 10.0f;
    ImGui::SliderFloat("##decay", &ui_state.decay, 1.0f, 1000.0f, "%.3f ms");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) ui_state.decay = 500.0f;
    ImGui::SliderFloat("##input_gain", &ui_state.input_gain, -20.0f, 20.0f, "%.3f dB");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) ui_state.input_gain = 0.0f;
    ImGui::SliderFloat("##output_gain", &ui_state.output_gain, -20.0f, 20.0f, "%.3f dB");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) ui_state.output_gain = 0.0f;

    ImGui::EndGroup();
    ImGui::PopItemWidth();
    
    // send updated module state to audio thread,
    // and request audio thread for analytics
    {
        message_t new_msg;

        if (!waiting) {
            new_msg.type = message_t::RequestAnalytics;
            if (process_queue.post(&new_msg, sizeof(new_msg))) {
                dbg("WARNING: LimiterModule process queue is full!\n");
            } else {
                waiting = true;
            }
        }

        new_msg.type = message_t::ModuleState;
        new_msg.mod_state = ui_state;
        if (process_queue.post(&new_msg, sizeof(new_msg))) {
            dbg("WARNING: LimiterModule process queue is full!\n");
        }
    }
}
