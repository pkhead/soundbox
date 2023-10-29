#pragma once
#include "util.h"

size_t convert_from_stereo(float* src, float** dest, size_t channel_count, size_t frames_per_buffer, bool interleave);
void convert_to_stereo(float** src, float* dest, size_t channel_count, size_t frames_per_buffer, bool interleave);

// 2nd-order IIR filters
class Filter2ndOrder
{
public:
    // [channel][order]
    float x[3]; // input
    float y[3]; // output

    // filter coefficients
    float a[3];
    float b[3];

    Filter2ndOrder();

    void low_pass(float sample_rate, float frequency, float linear_gain);
    void high_pass(float sample_rate, float frequency, float linear_gain);
    void all_pass(float sample_rate, float frequency, float linear_gain);
    void peak(float sample_rate, float frequency, float linear_gain, float bandwidth);

    void low_shelf(float sample_rate, float frequency, float gain, float slope);
    void high_shelf(float sample_rate, float frequency, float gain, float slope);

    void process(float* sample);

    float attenuation(float hz, float sample_rate);
};

template <class T = float>
class DelayLine
{
private:
    size_t index;
    size_t _max_size;
    T* buf;

public:
    size_t delay;

    DelayLine()
    :   index(0),
        buf(nullptr),
        _max_size(0),
        delay(0)
    {}

    DelayLine(size_t max_size)
    : DelayLine()
    {
        resize(max_size);
    }

    ~DelayLine()
    {
        if (buf) {
            delete[] buf;
        }
    }

    void resize(size_t new_capacity)
    {
        if (buf) {
            delete[] buf;
        }

        _max_size = new_capacity;

        // create new zero-initialized array
        buf = new float[_max_size];
        memset(buf, 0, _max_size * sizeof(float));
    }

    void clear()
    {
        memset(buf, 0, _max_size * sizeof(float));
    }

    inline float read() {
        return buf[index];
    }

    inline void write(float v) {
        buf[index++] = v;
        if (index > delay) index = 0;
    }

    inline size_t max_size() const { return _max_size; }
};