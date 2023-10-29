#include "dsp.h"

#include <cstring>

size_t convert_from_stereo(float* src, float** dest, size_t channel_count, size_t frames_per_buffer, bool interleave)
{
    size_t sample_count;
    size_t buffer_size = frames_per_buffer * 2;

    if (channel_count == 1)
    {
        float* dest_mono = dest[0];

        if (interleave)
        {
            // send interleaved buffer
            sample_count = buffer_size;
            memcpy(dest_mono, src, buffer_size);
        }
        else // convert to mono
        {
            sample_count = frames_per_buffer;
            for (size_t i = 0; i < frames_per_buffer; i++)
                dest_mono[i] = src[i * 2] + src[i * 2 + 1];
        }
    }

    // separate each channel
    else if (channel_count == 2)
    {
        sample_count = frames_per_buffer;

        for (size_t i = 0; i < frames_per_buffer; i++)
        {
            dest[0][i] = src[i * 2];
            dest[1][i] = src[i * 2 + 1];
        }
    }

    // unsupported channel count, initialize buffers to 0
    else
    {
        sample_count = frames_per_buffer;

        for (int c = 0; c < channel_count; c++)
            for (int i = 0; i < frames_per_buffer; i++)
                dest[c][i] = 0.0f;
    }

    return sample_count;
}

void convert_to_stereo(float** src, float* dest, size_t channel_count, size_t frames_per_buffer, bool interleave)
{
    size_t buffer_size = frames_per_buffer * 2;

    // mono
    if (channel_count == 1)
    {
        float* buf = src[0];

        if (interleave) {
            memcpy(dest, src, buffer_size);
        } else {
            for (size_t i = 0; i < frames_per_buffer; i++) {
                dest[i * 2] = buf[i];
                dest[i * 2 + 1] = buf[i];
            }
        }
    }

    // stereo
    else if (channel_count == 2)
    {
        for (size_t i = 0; i < frames_per_buffer; i++) {
            dest[i * 2] = src[0][i];
            dest[i * 2 + 1] = src[1][i];
        }
    }

    // unsupported channel count, set to 0
    else
    {
        memset(dest, 0, buffer_size * sizeof(float));
    }
}

/*
 Filters
 Thanks to https://webaudio.github.io/Audio-EQ-Cookbook/Audio-EQ-Cookbook.txt
*/
Filter2ndOrder::Filter2ndOrder()
{
    for (int i = 0; i < 3; i++)
    {
        x[i] = 0.0f;
        y[i] = 0.0f;
        a[i] = 0.0f;
        b[i] = 0.0f;
    }
}

void Filter2ndOrder::process(float* value)
{
        x[0] = *value;
        y[0] = b[0] * x[0] + b[1] * x[1] + b[2] * x[2]
                                 - a[1] * y[1] - a[2] * y[2];

        x[2] = x[1];
        x[1] = x[0];
        y[2] = y[1];
        y[1] = y[0];

        *value = y[0];
}

void Filter2ndOrder::low_pass(float Fs, float f0, float Q)
{
    // Fs: sample rate
    // f0: frequency
    // Q: peak linear gain
    float w0 = 2.0f * M_PI * f0 / Fs;
    float si = sinf(w0);
    float co = cosf(w0);

    float alpha = si / (2.0f * Q);
    float a0 = 1.0f + alpha;

    b[0] = (1.0f - co) / (2.0f * a0);
    b[1] = (1.0f - co) / a0;
    b[2] = (1.0f - co) / (2.0f * a0);
    a[0] = 1.0f;
    a[1] = (-2.0f * co) / a0;
    a[2] = (1.0f - alpha) / a0;
}

void Filter2ndOrder::high_pass(float Fs, float f0, float Q)
{
    float w0 = 2.0f * M_PI * f0 / Fs;
    float si = sinf(w0);
    float co = cosf(w0);

    float alpha = si / (2.0f * Q);
    float a0 = 1.0f + alpha;

    b[0] =  (1.0f + co) / (2.0f * a0);
    b[1] = -(1.0f + co) / a0;
    b[2] =  (1.0f + co) / (2.0f * a0);
    a[0] =  1.0f;
    a[1] =  (-2.0f * co) / a0;
    a[2] =  (1.0f - alpha) / a0;
}

void Filter2ndOrder::all_pass(float Fs, float f0, float Q)
{
    float w0 = 2.0f * M_PI * f0 / Fs;
    float si = sinf(w0);
    float co = cosf(w0);
    float alpha = si / (2.0f * Q);
    float a0 = 1.0f + alpha;

    b[0] = a[2] = (1.0f - alpha) / a0;
    b[1] = a[1] = (-2.0f * co) / a0;
    b[2] = a[0] = 1.0f;
}

void Filter2ndOrder::low_shelf(float Fs, float f0, float gain, float slope)
{
    float A = sqrtf(powf(10.0f, gain / 40.0f));
    float w0 = 2.0f * M_PI * f0 / Fs;

    float si = sinf(w0);
    float co = cosf(w0);
    float alpha = si / 2.0f * sqrtf( (A + 1.0f / A) * (1.0f / slope - 1.0f) + 2.0f );
    float sqrt_alpha = 2.0f * sqrtf(A) * alpha;
    
    float a0 =             (A + 1.0f) + (A - 1.0f) * co + sqrt_alpha;
    b[0] =       (     A*( (A + 1.0f) - (A - 1.0f) * co + sqrt_alpha)) / a0;
    b[1] =       (2.0f*A*( (A - 1.0f) - (A + 1.0f) * co))              / a0;
    b[2] =       (     A*( (A + 1.0f) - (A - 1.0f) * co - sqrt_alpha)) / a0;
    a[0] =       1.0f;
    a[1] =       ( -2.0f*( (A - 1.0f) + (A + 1.0f) * co))              / a0;
    a[2] =       (         (A + 1.0f) + (A - 1.0f) * co - sqrt_alpha)  / a0;
}

void Filter2ndOrder::high_shelf(float Fs, float f0, float gain, float slope)
{
    float A = sqrtf(powf(10.0f, gain / 40.0f));
    float w0 = 2.0f * M_PI * f0 / Fs;

    float si = sinf(w0);
    float co = cosf(w0);
    float alpha = si / 2.0f * sqrtf( (A + 1.0f / A) * (1.0f / slope - 1.0f) + 2.0f );
    float sqrt_alpha = 2.0f * sqrtf(A) * alpha;
    
    float a0 =             (A + 1.0f) - (A - 1.0f) * co + sqrt_alpha;
    b[0] =       (      A*( (A + 1.0f) + (A - 1.0f) * co + sqrt_alpha)) / a0;
    b[1] =       (-2.0f*A*( (A - 1.0f) + (A + 1.0f) * co))              / a0;
    b[2] =       (      A*( (A + 1.0f) + (A - 1.0f) * co - sqrt_alpha)) / a0;
    a[0] =       1.0f;
    a[1] =       (   2.0f*( (A - 1.0f) - (A + 1.0f) * co))              / a0;
    a[2] =       (          (A + 1.0f) - (A - 1.0f) * co - sqrt_alpha)  / a0;
}

void Filter2ndOrder::peak(float Fs, float f0, float gain, float bw_scale)
{
    // Fs: sample rate
    // f0: frequency
    // gain: peak linear gain
    // bw: band width
    
    float w0 = 2.0 * M_PI * f0 / Fs;
    float sqrt_gain = sqrtf(powf(10.0f, gain / 40.0f));
    float si = sinf(w0);
    float co = cosf(w0);
    float alpha = si * sinh(log(2.0f) / 2.0f * bw_scale * w0 / si);
    float a0 = 1.0f + alpha / sqrt_gain;
    //float bandwidth = bw_scale * w0 / (sqrt_gain >= 1.0f ? sqrt_gain : 1.0f / sqrt_gain);
    //float alpha = tanf(bw_scale * 0.5f);
    //float a0 = 1.0f + alpha / sqrt_gain;
    
    b[0] = (1.0f + alpha * sqrt_gain) / a0;
    b[1] = a[1] = -2.0f * cosf(w0) / a0;
    a[0] = 1.0f;
    b[2] = (1.0f - alpha * sqrt_gain) / a0;
    a[2] = (1.0f - alpha / sqrt_gain) / a0;
}

// filter analysis
// copied from beepbox source code,
// although i probably should try to
// understand more of the theory
// behind this
float Filter2ndOrder::attenuation(float hz, float sample_rate)
{
    float corner_rad_per_sample = 2.0f * M_PI * hz / sample_rate;
    float real = cosf(corner_rad_per_sample);
    float imag = sinf(corner_rad_per_sample);

    const float z1_r = real;
    const float z1_i = -imag;
    float num_r = b[0] + b[1] * z1_r;
    float num_i = b[1] * z1_i;
    float denom_r = 1.0 + a[1] * z1_r;
    float denom_i = a[1] * z1_i;
    float z_r = z1_r;
    float z_i = z1_i;

    for (int i = 2; i <= 2; i++) {
        const float realTemp = z_r * z1_r - z_i * z1_i;
        const float imagTemp = z_r * z1_i + z_i * z1_r;
        z_r = realTemp;
        z_i = imagTemp;
        num_r += b[i] * z_r;
        num_i += b[i] * z_i;
        denom_r += a[i] * z_r;
        denom_i += a[i] * z_i;
    }

    float denom = denom_r * denom_r + denom_i * denom_i;
    real = num_r * denom_r + num_i * denom_i;
    imag = num_i * denom_r - num_r * denom_i;

    return sqrtf(real*real + imag*imag) / denom;
}
