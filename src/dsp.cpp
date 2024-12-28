#include <cstring>
#include <cmath>
#include <util.hpp>
#include "dsp.hpp"

using namespace dsp;

/*
* ADSR
* Attack. Decay. Sustain. Release.
**/
ADSR::ADSR() {}
ADSR::Instance::Instance() {}

ADSR::ADSR(float a, float d, float s, float r)
:   attack(a), decay(d), sustain(s), release(r)
{}

inline static float compute_multiplier(float start, float end, int release_time) {
    return 1.0f + logf(end / start) / release_time;
}

bool ADSR::Instance::compute(int sample_rate, float &out, const ADSR &params) {
    if (stage == 4) {
        //lerp_from = value;
        //lerp_to = 0.0f;
        stage = 3;

        if (params.release == 0.0f) {
            samples_remaining = 0;
            multiplier = 0.0f;
        } else {
            multiplier = compute_multiplier(value, MINIMUM_LEVEL, params.release * sample_rate);
            samples_remaining = params.release * sample_rate;
        }
    }

    if (samples_remaining >= 0 && samples_remaining-- == 0) {
        switch (stage) {
            case 0: // start attack
                if (params.attack > 0.0f) {
                    value = MINIMUM_LEVEL;
                    multiplier = compute_multiplier(MINIMUM_LEVEL, 1.0f, params.attack * sample_rate);
                    //lerp_from = 0.0f;
                    //lerp_to = 1.0f;
                    samples_remaining = params.attack * sample_rate;
                    stage = 1;
                    break;
                }
            
            case 1: // start decay
                if (params.decay > 0.0f) {
                    value = 1.0f;
                    multiplier = compute_multiplier(1.0f, util::max(MINIMUM_LEVEL, params.sustain), params.decay * sample_rate);
                    //lerp_from = 1.0f;
                    //lerp_to = params.sustain;
                    samples_remaining = params.decay * sample_rate;
                    stage = 2;
                    break;
                }
            
            case 2: // sustain
                //lerp_from = params.sustain;
                //lerp_to = params.sustain;
                value = params.sustain;
                multiplier = 1.0f;
                samples_remaining = -1;
                stage = 2;
                break;
            
            case 3: // release finished, note ended
                samples_remaining = 0;
                return true; 
        }
    }

    //value += multiplier * value;
    value *= multiplier;
    if (value <= MINIMUM_LEVEL) {
        out = 0.0f;
    } else {
        out = value;
    }
    value = util::clamp(MINIMUM_LEVEL, 1.0f, value);
    //out = value = (lerp_to - lerp_from) * t + lerp_from;
    
    return false;
}

// multipler = 1.0 + (log(endLevel) - log(startLevel)) / lengthInSamples
// 3 iterations
void ADSR::Instance::release(const ADSR& params)
{
    stage = 4;
}









/*
 Filters
 Thanks to https://webaudio.github.io/Audio-EQ-Cookbook/Audio-EQ-Cookbook.txt
*/
FilterIIR2ndOrder::FilterIIR2ndOrder()
{
    for (int i = 0; i < 3; i++)
    {
        x[i] = 0.0f;
        y[i] = 0.0f;
        a[i] = 0.0f;
        b[i] = 0.0f;
    }
}

void FilterIIR2ndOrder::process(float* value)
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

void FilterIIR2ndOrder::low_pass(float Fs, float f0, float Q)
{
    // Fs: sample rate
    // f0: frequency
    // Q: peak linear gain
    float w0 = 2.0f * PIf * f0 / Fs;
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

void FilterIIR2ndOrder::high_pass(float Fs, float f0, float Q)
{
    float w0 = 2.0f * PIf * f0 / Fs;
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

void FilterIIR2ndOrder::all_pass(float Fs, float f0, float Q)
{
    float w0 = 2.0f * PIf * f0 / Fs;
    float si = sinf(w0);
    float co = cosf(w0);
    float alpha = si / (2.0f * Q);
    float a0 = 1.0f + alpha;

    b[0] = a[2] = (1.0f - alpha) / a0;
    b[1] = a[1] = (-2.0f * co) / a0;
    b[2] = a[0] = 1.0f;
}

void FilterIIR2ndOrder::low_shelf(float Fs, float f0, float gain, float slope)
{
    float A = sqrtf(powf(10.0f, gain / 40.0f));
    float w0 = 2.0f * PIf * f0 / Fs;

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

void FilterIIR2ndOrder::high_shelf(float Fs, float f0, float gain, float slope)
{
    float A = sqrtf(powf(10.0f, gain / 40.0f));
    float w0 = 2.0f * PIf * f0 / Fs;

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

void FilterIIR2ndOrder::peak(float Fs, float f0, float gain, float bw_scale)
{
    // Fs: sample rate
    // f0: frequency
    // gain: peak linear gain
    // bw: band width
    
    float w0 = 2.0 * PIf * f0 / Fs;
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
float FilterIIR2ndOrder::attenuation(float hz, float sample_rate)
{
    float corner_rad_per_sample = 2.0f * PIf * hz / sample_rate;
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








VUMeter::VUMeter(unsigned int sample_rate, float db_max) :
    sample_rate(sample_rate)
{
    level = 0.0f;
    peak = 0.0f;
    max = db_to_mult(db_max);
    wait_length = 0;
    buffer = new float[BUFFER_SIZE];
    bufidx = 0;

    memset(buffer, 0, BUFFER_SIZE * sizeof(float));
}

VUMeter::~VUMeter() {
    delete[] buffer;
}

void VUMeter::write(float value) {
    // write value to buffer
    buffer[bufidx] = fabsf(value);

    static_assert((BUFFER_SIZE & (BUFFER_SIZE - 1)) == 0, "BUFFER_SIZE is not a power of 2");
    bufidx = (bufidx + 1) & (BUFFER_SIZE-1);
}

void VUMeter::update(unsigned int dt_frames) {
    // calculate level from buffer
    level = 0.0f;
    for (size_t i = 0; i < BUFFER_SIZE; i++) {
        if (buffer[i] > level) {
            level = buffer[i];
        }
    }
    
    level = util::min(level / max, 1.0f);

    if (level >= peak) {
        peak = level;
        wait_length = sample_rate;
    } else if (wait_length > 0) {
        wait_length -= dt_frames;
    } else {
        peak -= 1.5f / sample_rate * dt_frames;
        if (peak < 0.0f) peak = 0.0f;
    }
}