/*
i'm not smart enough to design my own reverb
so i looked at this blog post: https://signalsmith-audio.co.uk/writing/2021/lets-write-a-reverb/ 
*/

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

    // create delay buffers
    size_t buffer_size = (size_t)(dest.sample_rate * MAX_DELAY_LEN);

    for (int i = 0; i < REVERB_CHANNEL_COUNT; i++)
    {
        echoes[i].resize(buffer_size);
    }

    for (int i = 0; i < DIFFUSE_STEPS; i++)
    {
        diffuse_delays[i].resize(buffer_size);
    }

    echoes[0].delay = 0.1f * dest.sample_rate;
    echoes[1].delay = 0.11f * dest.sample_rate;
    echoes[2].delay = 0.12f * dest.sample_rate;
    echoes[3].delay = 0.13f * dest.sample_rate;
    echoes[4].delay = 0.14f * dest.sample_rate;
    echoes[5].delay = 0.15f * dest.sample_rate;
    echoes[6].delay = 0.16f * dest.sample_rate;
    echoes[7].delay = 0.17f * dest.sample_rate;
}

ReverbModule::~ReverbModule()
{}

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
            delayed[i] = echoes[i].read() * mix_mult;
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