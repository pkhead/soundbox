#include <imgui.h>
#include "../sys.h"
#include "audio.h"
#include "delay.h"
using namespace audiomod;

DelayModule::DelayModule(Song* song) : ModuleBase(song, true)
{
    id = "effects.delay";
    name = "Delay/Echo";

    delay_time = 0.25f;
    stereo_offset = 0.0f;
    feedback = 0.6f;
    mix = -0.0f;

    delay_line_index[0] = 0;
    delay_line_index[1] = 0;
    delay_line_size = 240000;
    delay_line[0] = new float[delay_line_size];
    delay_line[1] = new float[delay_line_size];

    for (size_t i = 0; i < delay_line_size; i++)
    {
        delay_line[0][i] = 0.0f;
        delay_line[1][i] = 0.0f;
    }
}

DelayModule::~DelayModule()
{
    delete[] delay_line[0];
    delete[] delay_line[1];
}

void DelayModule::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count)
{
    if (this->panic)
    {
        this->panic = false;

        for (size_t i = 0; i < delay_line_size; i++)
        {
            delay_line[0][i] = 0.0f;
            delay_line[1][i] = 0.0f;
        }
    }
    
    // cache atomic variables
    float mix = this->mix;
    float feedback = this->feedback;
    float delay_time = this->delay_time;
    float stereo_offset = this->stereo_offset;

    float delay_time_left = delay_time;
    float delay_time_right = delay_time;

    if (stereo_offset > 0.0f)
        delay_time_right += stereo_offset;
    else
        delay_time_left += -stereo_offset;
    
    // convert time sample index from sample rate
    float delay_in_samples[2];
    delay_in_samples[0] = delay_time_left * sample_rate;
    delay_in_samples[1] = delay_time_right * sample_rate;

    float wet_mix = (mix + 1.0f) / 2.0f;
    float dry_mix = 1.0f - wet_mix;

    float input_frame[2];
    float delay_frame[2];

    for (size_t i = 0; i < buffer_size; i += 2)
    {
        delay_frame[0] = delay_line[0][delay_line_index[0]];
        delay_frame[1] = delay_line[1][delay_line_index[1]];

        // get data from inputs
        input_frame[0] = 0.0f;
        input_frame[1] = 0.0f;
        for (size_t j = 0; j < num_inputs; j++)
        {
            input_frame[0] += inputs[j][i];
            input_frame[1] += inputs[j][i + 1];
        }

        // mix dry and wet mix and write to output
        output[i] = (input_frame[0] * dry_mix) + (delay_frame[0] * wet_mix);
        output[i + 1] = (input_frame[1] * dry_mix) + (delay_frame[1] * wet_mix);

        // update delay line
        delay_line[0][delay_line_index[0]] = feedback * (delay_frame[0] + input_frame[0]);
        delay_line[1][delay_line_index[1]] = feedback * (delay_frame[1] + input_frame[1]);
        
        if (delay_line_index[0]++ >= delay_in_samples[0])
            delay_line_index[0] = 0;

        if (delay_line_index[1]++ >= delay_in_samples[1])
            delay_line_index[1] = 0;
    }
}

void DelayModule::_interface_proc()
{
    float mix = this->mix;
    float feedback = this->feedback;
    float delay_time = this->delay_time;
    float stereo_offset = this->stereo_offset;

    // labels
    ImGui::BeginGroup();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Dry/Wet Mix");
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Feedback");
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Delay");
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Stereo Offset");
    ImGui::EndGroup();

    // values
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::PushItemWidth(ImGui::GetTextLineHeight() * 10.0f);

    // dry/wet
    ImGui::SliderFloat("###mix", &mix, -1.0f, 1.0f);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle))
        mix = 0.0f;

    // feedback
    ImGui::SliderFloat("###feedback", &feedback, 0.0f, 1.0f);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle))
        feedback = 0.5f;

    // delay time
    ImGui::SliderFloat("###delay_l", &delay_time, 0.0f, 4.0f);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle))
        delay_time = 0.2f;

    // stereo offset
    ImGui::SliderFloat("###delay_r", &stereo_offset, -1.0f, 1.0f);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle))
        stereo_offset = 0.0f;

    ImGui::PopItemWidth();
    ImGui::EndGroup();

    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::Button("Stop"))
        panic = true;

    this->mix = mix;
    this->feedback = feedback;
    this->delay_time = delay_time;
    this->stereo_offset = stereo_offset;
}

struct DelayModuleState
{
    float delay_time;
    float stereo_offset;
    float feedback;
    float mix;
};

size_t DelayModule::save_state(void** output) const
{
    DelayModuleState* state = new DelayModuleState;
    state->delay_time = swap_little_endian((float) delay_time);
    state->stereo_offset = swap_little_endian((float) stereo_offset);
    state->feedback = swap_little_endian((float) feedback);
    state->mix = swap_little_endian((float) mix);
    *output = state;
    return sizeof(*state);
}

bool DelayModule::load_state(void* state_ptr, size_t size)
{
    if (size != sizeof(DelayModuleState)) return false;
    DelayModuleState* state = static_cast<DelayModuleState*>(state_ptr);

    delay_time = swap_little_endian(state->delay_time);
    stereo_offset = swap_little_endian(state->stereo_offset);
    feedback = swap_little_endian(state->feedback);
    mix = swap_little_endian(state->mix);
    return true;
}