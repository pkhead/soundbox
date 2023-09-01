#include <cstring>
#include <cassert>
#include "reverb.h"

using namespace audiomod;

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

ReverbModule::ReverbModule(DestinationModule& dest)
    : ModuleBase(dest, true)
{
    id = "effect.reverb";
    name = "Reverb";
    mix_mult = 0.5f;

    for (int i = 0; i < REVERB_CHANNEL_COUNT; i++)
        delay_index[i] = 0;

    delay_len[0] = 0.1f * dest.sample_rate;
    delay_len[1] = 0.11f * dest.sample_rate;
    delay_len[2] = 0.12f * dest.sample_rate;
    delay_len[3] = 0.13f * dest.sample_rate;
    delay_len[4] = 0.14f * dest.sample_rate;
    delay_len[5] = 0.15f * dest.sample_rate;
    delay_len[6] = 0.16f * dest.sample_rate;
    delay_len[7] = 0.17f * dest.sample_rate;

    // create delay buffers
    size_t buffer_size = (size_t)(dest.sample_rate * MAX_DELAY_LEN);

    for (int i = 0; i < REVERB_CHANNEL_COUNT; i++)
    {
        delay_bufs[i] = new float[buffer_size];
        memset(delay_bufs[i], 0, buffer_size * sizeof(float));
    }
}

ReverbModule::~ReverbModule()
{
    for (int i = 0; i < REVERB_CHANNEL_COUNT; i++)
    {
        delete[] delay_bufs[i];
    }
}

void ReverbModule::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count)
{
    float input_frame[2];
    float channels[REVERB_CHANNEL_COUNT];
    float delayed[REVERB_CHANNEL_COUNT];
    int k;

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

        // delay
        for (int i = 0; i < REVERB_CHANNEL_COUNT; i++)
        {
            delayed[i] = delay_bufs[i][delay_index[i]] * mix_mult;
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
            delay_bufs[i][delay_index[i]] = channels[i];
            delay_index[i] = (delay_index[i] + 1) % delay_len[i];
        }

        // mix internal channels into output
        k = 0;
        output[smp] = 0.0f;
        output[smp+1] = 0.0f;
        for (int i = 0; i < REVERB_CHANNEL_COUNT; i++)
        {
            output[smp+k] += channels[i];
            k = (k + 1) % 2;
        }
    }
}

void ReverbModule::_interface_proc()
{

}