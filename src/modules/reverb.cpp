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
    mix_mult = 0.8f;

    // create delay buffers
    size_t buffer_size = (size_t)(dest.sample_rate * MAX_DELAY_LEN);

    for (int i = 0; i < REVERB_CHANNEL_COUNT; i++)
    {
        echoes[i].resize(buffer_size);
    }

    assert(DIFFUSE_STEPS == 4);
    static float DELAY_MULTIPLIERS[] = {0.0f, 0.125f, 0.25f, 0.5f, 1.0f};

    for (int i = 0; i < DIFFUSE_STEPS; i++)
    {
        float range = 0.040f;
        float range_start = (float)i * range;

        for (int k = 0; k < REVERB_CHANNEL_COUNT; k++)
        {
            diffuse_delays[i][k].resize(buffer_size);
            float delay_len = ((double)rand() / RAND_MAX) * range + range_start;
            diffuse_delays[i][k].delay = (float)dest.sample_rate * delay_len;
            diffuse_factors[i][k] = (rand() > RAND_MAX / 2) ? 1.0f : -1.0f;
            //diffuse_delays[i][k].delay = ((double)rand() / RAND_MAX * 0.6) * dest.sample_rate;
        }
    }

    // setup echoes
    float d = 0.100f;

    for (int i = 0; i < REVERB_CHANNEL_COUNT; i++)
    {
        echoes[i].delay = d * dest.sample_rate;
        d = d + 0.01f;
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