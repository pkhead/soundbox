#include <cmath>
#include <cstdio>
#include <imgui.h>
#include <math.h>
#include "../sys.h"
#include "audio.h"
#include "delay.h"
#include "../song.h"

using namespace audiomod;

DelayModule::DelayModule(Song* song) : ModuleBase(song, true)
{
    id = "effects.delay";
    name = "Delay";

    delay_time = 0.25;
    tempo_division = 0;
    stereo_offset = 0.0f;
    delay_mode = false;
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

static double division_to_secs(float tempo, int division_enum)
{
    int exponent = division_enum / 3 - 6;
    int div_type = division_enum % 3;
    float len;

    if (div_type == 0) // normal beats
        len = powf(2.0f, exponent);
    else if (div_type == 1) // dotted beats
    {
        len = powf(2.0f, exponent);
        len += len / 2.0f;
    }
    else if (div_type == 2) // triplets
        len = powf(3.0f / 2.0f, exponent);

    return len * (60.0f / tempo);
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
    double delay_time = this->delay_time;
    float stereo_offset = this->stereo_offset;

    if (delay_mode)
        delay_time = division_to_secs(song->tempo, tempo_division);

    double delay_time_left = delay_time;
    double delay_time_right = delay_time;

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

static const char* DIVISION_NAMES[] = {
    "1/64", // beats
    "1/64 dotted", // dotted
    "1/48", // triplet
    "1/32", // beats
    "1/32 dotted", // dotted
    "1/24", // triplet
    "1/16", // beats
    "1/16 dotted", // dotted
    "1/12", // triplet
    "1/8", // beats
    "1/8 dotted", // dotted
    "1/6", // triplet
    "1/4", // beats
    "1/4 dotted", // dotted
    "1/3", // triplet
    "1/2", // beats
    "1/2 dotted", // dotted
    "2/3", // triplet
    "1/1", // beats
    "1/1 dotted", // dotted
    "3/3", // triplet
    "2/1", // beats
    "2/1 dotted", // dotted
    "6/3", // triplet
    "4/1", // beats
    "4/1 dotted", // dotted
    "12/3", // triplet
    "8/1",  // beats
    "8/1 dotted", // dotted
    "24/3", // triplet
};

void DelayModule::_interface_proc()
{
    float mix = this->mix;
    float feedback = this->feedback;
    double delay_time = this->delay_time;
    float stereo_offset = this->stereo_offset;
    bool delay_mode = this->delay_mode;

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

    if (delay_mode)
    {
        // delay time in beats
        int tempo_division = this->tempo_division;
        ImGui::SliderInt("###tempo_division", &tempo_division, 0, (10 * 3) - 1, DIVISION_NAMES[tempo_division]);  
        this->tempo_division = tempo_division;
    }
    else
    {
        // delay time in seconds
        float delay_time_float = delay_time;
        ImGui::SliderFloat("###delay", &delay_time_float, 0.0f, 4.0f);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Middle))
            delay_time_float = 0.2f;
        delay_time = delay_time_float;
    }

    // stereo offset
    ImGui::SliderFloat("###delay_r", &stereo_offset, -1.0f, 1.0f);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle))
        stereo_offset = 0.0f;

    ImGui::PopItemWidth();
    ImGui::EndGroup();

    if (ImGui::Button("Stop"))
        panic = true;
    ImGui::SameLine();
    ImGui::Text("Use Tempo");
    ImGui::SameLine();
    if (ImGui::Checkbox("###mode", &delay_mode))
    {
        // TODO: set tempo_division to nearest delay_mode
        if (delay_mode)
        {

        }
        else
            delay_time = division_to_secs(song->tempo, tempo_division);
    }

    this->delay_mode = delay_mode;
    this->mix = mix;
    this->feedback = feedback;
    this->delay_time = delay_time;
    this->stereo_offset = stereo_offset;
}

struct DelayModuleState
{
    double delay_time;
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