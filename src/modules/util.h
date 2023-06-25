#pragma once
#include <atomic>
#include <cstddef>

// debug log
#ifdef _NDEBUG

#define dbg(fmt, ...)

#else

#include <cstdio>
#define dbg(fmt, ...) printf(fmt, __VA_ARGS__)

#endif

inline int sign(float v) {
    return v >= 0.0f ? 1 : -1; 
}

inline bool is_zero_crossing(float prev, float next) {
    return (prev == 0.0f && next == 0.0f) || (sign(prev) != sign(next));
}

// a RingBuffer of floats
// TODO: make audio.cpp use this so there aren't two implementations of ring buffers
// in the same project (which is a waste!)
class RingBuffer
{
private:
    size_t audio_buffer_capacity = 0;
    float* audio_buffer = nullptr;
    std::atomic<size_t> buffer_write_ptr = 0;
    std::atomic<size_t> buffer_read_ptr = 0;

public:
    RingBuffer(size_t capacity);
    ~RingBuffer();
    
    size_t queued() const;

    void write(float* buf, size_t size);
    size_t read(float* out, size_t size);
};

// TODO: filters
// TODO: fft