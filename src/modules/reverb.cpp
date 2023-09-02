/*
i'm not smart enough to design my own reverb
so i looked at this blog post: https://signalsmith-audio.co.uk/writing/2021/lets-write-a-reverb/ 
*/

#include <cstring>
#include <cmath>
#include <cassert>
#include <imgui.h>
#include "reverb.h"

using namespace audiomod;

// householder matrix
static void householder(float* arr, size_t size)
{
    float multiplier = -2.0f / size; 
    float sum = 0;

    for (int i = 0; i < size; i++)
    {
        sum += arr[i];
    }

    sum *= multiplier;

    for (int i = 0; i < size; i++)
    {
        arr[i] += sum;
    }
}

// hadamard matrix, size must be a power of 2
static void recursive_unscaled(float* arr, size_t size)
{
    if (size <= 1) return;
    int h_size = size / 2;

    // two (unscaled) Hadamards of half the size
    recursive_unscaled(arr, h_size);
    recursive_unscaled(arr + h_size, h_size);

    // combine the two halves using sum/difference
    for (int i = 0; i < h_size; i++)
    {
        float a = arr[i];
        float b = arr[i + h_size];
        arr[i] = a + b;
        arr[i + h_size] = a - b;
    }
}

template <size_t size>
static void hadamard(float* arr)
{
    recursive_unscaled(arr, size);
    float factor = sqrtf(1.0f / size);

    for (int c = 0; c < size; c++)
    {
        arr[c] *= factor;
    }
}

ReverbModule::ReverbModule(DestinationModule& dest)
    : ModuleBase(dest, true),
      state_queue(sizeof(module_state), 8)
{
    id = "effect.reverb";
    name = "Reverb";
    
    // initialize state
    ui_state.mix = 0.0f;
    ui_state.shelf_freq = 200.0f;
    ui_state.feedback = 0.8f;
    ui_state.echo_delay = 0.5f;
    ui_state.shelf_gain = -10.0f;
    process_state = ui_state;

    // create delay buffers
    size_t buffer_size = (size_t)(dest.sample_rate * MAX_DELAY_LEN);

    for (int i = 0; i < REVERB_CHANNEL_COUNT; i++)
    {
        echoes[i].resize(buffer_size);
    }

    for (int i = 0; i < DIFFUSE_STEPS; i++)
    {
        float range = 0.040f;
        float range_start = (float)i * range;

        for (int k = 0; k < REVERB_CHANNEL_COUNT; k++)
        {
            diffuse_delays[i][k].resize(buffer_size);
            float delay_len = ((double)rand() / RAND_MAX) * range + range_start;
            assert(delay_len <= MAX_DELAY_LEN);
            diffuse_delays[i][k].delay = (float)dest.sample_rate * delay_len;
            diffuse_factors[i][k] = (rand() > RAND_MAX / 2) ? 1.0f : -1.0f;
            //diffuse_delays[i][k].delay = ((double)rand() / RAND_MAX * 0.6) * dest.sample_rate;
        }
    }
}

ReverbModule::~ReverbModule()
{}

void ReverbModule::diffuse(int i, float* values)
{
    float temp[REVERB_CHANNEL_COUNT];

    // delay values
    for (size_t c = 0; c < REVERB_CHANNEL_COUNT; c++)
    {
        temp[c] = diffuse_delays[i][c].read();
        diffuse_delays[i][c].write(values[c]);
    }

    // apply mixing matrix
    hadamard<REVERB_CHANNEL_COUNT>(temp);

    // shuffle & polarity inversion
    for (size_t c = 0; c < REVERB_CHANNEL_COUNT; c++)
    {
        values[c] = temp[c] * diffuse_factors[i][c];
    }
}

void ReverbModule::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count)
{
    // receive new state sent from ui thread
    while (true)
    {
        MessageQueue::read_handle_t handle = state_queue.read();
        if (!handle) break;

        assert(handle.size() == sizeof(module_state));
        module_state* state = (module_state*) handle.data();

        process_state = *state;
    }

    // setup echo delays
    for (int i = 0; i < REVERB_CHANNEL_COUNT; i++)
    {
        float d = (float)i / REVERB_CHANNEL_COUNT * process_state.echo_delay;
        assert(d <= MAX_DELAY_LEN);
        echoes[i].delay = d * _dest.sample_rate;
    }

    // setup filters
    for (int i = 0; i < REVERB_CHANNEL_COUNT; i++)
    {
        shelf_filters[i].high_shelf(_dest.sample_rate, process_state.shelf_freq, process_state.shelf_gain, 0.5f);
    }
    
    float input_frame[2], out_frame[2];
    float channels[REVERB_CHANNEL_COUNT];
    float delayed[REVERB_CHANNEL_COUNT];
    int k;

    float dry = 1.0f - process_state.mix;
    float wet = process_state.mix;

    for (size_t smp = 0; smp < buffer_size; smp += 2)
    {
        // obtain input frame
        input_frame[0] = 0.0f;
        input_frame[1] = 0.0f;

        for (size_t k = 0; k < num_inputs; k++)
        {
            input_frame[0] += inputs[k][smp];
            input_frame[1] += inputs[k][smp+1];
        }

        // split input frame into internal channels
        k = 0;
        for (int i = 0; i < REVERB_CHANNEL_COUNT; i++)
        {
            channels[i] = input_frame[k];
            k = (k + 1) % 2;
        }

        // diffuse step
        for (int i = 0; i < DIFFUSE_STEPS; i++)
        {
            diffuse(i, channels);
        }

        // delay with feedback & filter
        for (int i = 0; i < REVERB_CHANNEL_COUNT; i++)
        {
            delayed[i] = echoes[i].read() * process_state.feedback;
            shelf_filters[i].process(delayed + i);
        }

        // apply mix matrix
        householder(delayed, REVERB_CHANNEL_COUNT);

        // mix with source
        for (int i = 0; i < REVERB_CHANNEL_COUNT; i++)
        {
            channels[i] += delayed[i];
        }

        // send to delay
        for (int i = 0; i < REVERB_CHANNEL_COUNT; i++)
        {
            echoes[i].write(channels[i]);
        }

        // mix internal channels into output
        k = 0;
        out_frame[0] = 0.0f;
        out_frame[1] = 0.0f;
        for (int i = 0; i < REVERB_CHANNEL_COUNT; i++)
        {
            out_frame[k] += out_frame[i];
            k = (k + 1) % 2;
        }

        output[smp] = input_frame[0] * dry + out_frame[0] * wet;
        output[smp+1] = input_frame[1] * dry + out_frame[1] * wet;
    }
}

void ReverbModule::_interface_proc()
{
    module_state old_state = ui_state;

    float width = ImGui::GetTextLineHeight() * 14.0f;
    ImGui::PushItemWidth(width);

    // labels
    ImGui::BeginGroup();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Dry/Wet Mix");
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Echo Delay");
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Feedback");
    ImGui::AlignTextToFramePadding();
    ImGui::Text("High Shelf Freq.");
    ImGui::AlignTextToFramePadding();
    ImGui::Text("High Shelf Gain");
    ImGui::EndGroup();
    ImGui::SameLine();

    // controls
    ImGui::BeginGroup();

    // mix
    ImGui::SliderFloat("##mix", &ui_state.mix, 0.0f, 1.0f);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) ui_state.mix = 0.0f;
    
    // delay
    ImGui::SliderFloat("##delay", &ui_state.echo_delay, 0.0f, 1.0f, "%.3f s");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) ui_state.echo_delay = 0.5f;
    
    // feedback
    ImGui::SliderFloat("##feedback", &ui_state.feedback, 0.0f, 1.0f);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) ui_state.feedback = 0.8f;
    
    // shelf freq
    ImGui::SliderFloat(
        "##shelf_freq", &ui_state.shelf_freq, 20.0f, _dest.sample_rate / 2.5f, "%.0f Hz",
        ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_NoRoundToFormat
    );
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) ui_state.shelf_freq = 200.0f;

    // shelf gain
    ImGui::SliderFloat(
        "##shelf_gain", &ui_state.shelf_gain, -20.0f, 5.0f, "%.3f dB",
        ImGuiSliderFlags_NoRoundToFormat
    );
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) ui_state.shelf_gain = -10.0f;

    ImGui::EndGroup();

    // if state changed, post new state
    if (memcmp(&old_state, &ui_state, sizeof(module_state)) != 0)
    {
        state_queue.post(&ui_state, sizeof(ui_state));
    }
}