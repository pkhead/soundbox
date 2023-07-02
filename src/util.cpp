#include <cstring>
#include <cmath>
#include <stdexcept>

#include "util.h"

RingBuffer::RingBuffer(size_t capacity)
{
    audio_buffer_capacity = capacity;
    audio_buffer = new float[audio_buffer_capacity];
    buffer_write_ptr = 0;
    buffer_read_ptr = 0;
}

RingBuffer::~RingBuffer()
{
    delete[] audio_buffer;
}

void RingBuffer::write(float* buf, size_t size)
{
    size_t write_ptr = buffer_write_ptr;

    // TODO: use memcpy to copy one or two chunks of the buffer
    // wrapping around the ring buffer
    for (size_t i = 0; i < size; i++)
    {
        audio_buffer[write_ptr++] = buf[i];
        write_ptr %= audio_buffer_capacity;
    }

        /*
    if (write_ptr + buf_size >= audio_buffer_capacity)
    {
    }
    else
    {
        // no bounds in sight
        memcpy(audio_buffer + write_ptr, buf, buf_size * sizeof(float));
        write_ptr += buf_size;
    }*/

    buffer_write_ptr = write_ptr;
}

size_t RingBuffer::read(float* out, size_t size)
{
    size_t num_read = 0;
    size_t write_ptr = buffer_write_ptr;
    size_t read_ptr = buffer_read_ptr;

    for (size_t i = 0; i < size; i++)
    {
        // break if there is no data more in buffer
        if (read_ptr == write_ptr - 1)
            break;
        
        else
        {
            out[i] = audio_buffer[read_ptr++];
            read_ptr %= audio_buffer_capacity;
        }

        num_read++;
    }

    buffer_read_ptr = read_ptr;
    return num_read;
}

size_t RingBuffer::queued() const
{
    size_t write_ptr = buffer_write_ptr;
    size_t read_ptr = buffer_read_ptr;

    if (write_ptr >= read_ptr) {
        return write_ptr - read_ptr;
    } else {
        return audio_buffer_capacity - read_ptr - 1 + write_ptr;
    }
}

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

// apdated from
// https://github.com/d1vanov/Simple-FFT/tree/master
void fft(complex_t<float>* data, int data_size, bool inverse) // data is array of complex numbers
{
    if ((data_size & data_size - 1) != 0) {
        dbg("ERROR: size is not a power of 2");
        throw std::runtime_error("size is not a power of 2");
    }

    // rearrange data first
    int target_index = 0;
    int bit_mask;

    for (int i = 0; i < data_size; i++)
    {
        if (target_index > i) {
            complex_t temp = data[target_index];
            data[target_index] = data[i];
            data[i] = temp;
        }

        bit_mask = data_size;

        while (target_index & (bit_mask >>= 1))
            target_index = target_index & (~bit_mask);

        target_index |= bit_mask;
    }

    // perform algorithm
    float pi = inverse ? -M_PI : -M_PI;
    int next, match;
    float sine, delta;
    complex_t mult, factor, product;

    for (int i = 1; i < data_size; i *= 2)
    {
        int next = i << 1; // getting the next bit
        delta = pi / i; // angle increasing
        sine = sinf(delta / 2.0f); // supplementary sine
        // multiplier for trigonometric reference
        mult = complex_t(-2.0f * sine * sine, sinf(delta));
        factor = complex_t(1.0f); // start transform factor

        // iterations through groups with
        // different transform factor
        for (int j = 0; j < i; j++) {
            // iterations through pairs within groups
            for (int k = j; k < data_size; k += next)
            {
                match = k + i;
                product = data[match] * factor;
                data[match] = data[k] - product;
                data[k] = data[k] + product;
            }

            factor = mult * factor + factor;
        }
    }

    if (inverse)
    {
        for (int i = 0; i < data_size; i++)
            data[i] = data[i] / data_size;
    }
}

// Filters
Filter2ndOrder::Filter2ndOrder()
{
    for (int c = 0; c < 2; c++)
    {
        for (int i = 0; i < 3; i++)
        {
            x[c][i] = 0.0f;
            y[c][i] = 0.0f;
            a[c][i] = 0.0f;
            b[c][i] = 0.0f;
        }
    }
}

void Filter2ndOrder::process(float input[2], float output[3])
{
    for (int c = 0; c < 2; c++)
    {
        x[c][0] = input[c];
        y[c][0] =
            b[c][0] * x[c][0] +
            b[c][1] * x[c][1] +
            b[c][2] * x[c][2] -
            a[c][1] * y[c][1] -
            a[c][2] - y[c][2];

        x[c][2] = x[c][1];
        x[c][1] = x[c][0];
        y[c][2] = y[c][1];
        y[c][1] = y[c][0];
    }
}

void Filter2ndOrder::low_pass(float Fs, float f0, float Q)
{
    // Fs: sample rate
    // f0: frequency
    // Q: peak linear gain
    float w0 = 2.0f * M_PI * f0 / Fs;
    float sin = sinf(w0);
    float cos = cosf(w0);

    float alpha = sin / (2.0f * Q);
    float a0 = 1.0f + alpha;

    for (int c = 0; c < 2; c++)
    {
        b[c][0] = (1.0f - cos) / (2.0f * a0);
        b[c][1] = (1.0f - cos) / a0;
        b[c][2] = (1.0f - cos) / (2.0f * a0);
        a[c][0] = 1.0f;
        a[c][1] = (-2.0f * cos) / a0;
        a[c][2] = (1.0f - alpha) / a0;
    }
}