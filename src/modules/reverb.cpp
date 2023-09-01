/*
i'm not smart enough to design my own reverb
so i looked at this blog post: https://signalsmith-audio.co.uk/writing/2021/lets-write-a-reverb/ 
*/

#include <cstring>
#include <cmath>
#include <cassert>
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
        float d = 0.001;

        for (int k = 0; k < REVERB_CHANNEL_COUNT; k++)
        {
            diffuse_delays[i][k].resize(buffer_size);
            diffuse_delays[i][k].delay = d * dest.sample_rate;
            d = d * 2.0f;
        }
    }

    // setup echoes
    float d = 0.05;

    for (int i = 0; i < REVERB_CHANNEL_COUNT; i++)
    {
        echoes[i].delay = d * dest.sample_rate;
        d = d + 0.1;
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
        diffuse_delays[i][c].write(values[c]);
        temp[c] = diffuse_delays[i][c].read();
    }

    // shuffle & polarity inversion
    assert(REVERB_CHANNEL_COUNT == 8);
    values[0] = temp[2];
    values[1] = -temp[3];
    values[2] = temp[1];
    values[3] = -temp[7];
    values[4] = -temp[6];
    values[5] = temp[4];
    values[6] = temp[5];
    values[7] = -temp[0];

    // apply mixing matrix
    hadamard<REVERB_CHANNEL_COUNT>(values);
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

        // diffuse step
        for (int i = 0; i < DIFFUSE_STEPS; i++)
        {
            diffuse(i, channels);
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