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