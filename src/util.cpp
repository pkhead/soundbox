#include <cstring>

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
}