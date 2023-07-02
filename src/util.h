#pragma once
#include <cstddef>
#include <vector>
#include <atomic>

// debug log
#ifdef _NDEBUG

#define dbg(...)
#define __BREAK__

#else

#include <cstdio>
#include <cstdarg>

#define dbg(...) __dbg(__FILE__, __LINE__, __VA_ARGS__)

__attribute__((__format__(__printf__, 3, 4)))
inline void __dbg(const char* file, int line, const char* fmt, ...) {
    printf("%s:%i: ", file, line);

    va_list vl;
    va_start(vl, fmt);
    vprintf(fmt, vl);
    va_end(vl);
}

#ifdef _WIN32
#define DBGBREAK __debugbreak
#else
#include <signal.h>
#define DBGBREAK raise(SIGTRAP)
#endif

#endif

inline int sign(float v) {
    return v >= 0.0f ? 1 : -1; 
}

inline bool is_zero_crossing(float prev, float next) {
    return (prev == 0.0f && next == 0.0f) || (sign(prev) != sign(next));
}

template <typename T>
inline T min(T a, T b) {
    return a < b ? a : b;
}

template <typename T>
inline T max(T a, T b) {
    return a > b ? a : b;
}

size_t convert_from_stereo(float* src, float** dest, size_t channel_count, size_t frames_per_buffer, bool interleave);
void convert_to_stereo(float** src, float* dest, size_t channel_count, size_t frames_per_buffer, bool interleave);

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

// a complex number
template <typename T = float>
struct complex_t
{
    T real;
    T imag;

    inline constexpr complex_t()                : real(0.0f), imag(0.0f) {}
    inline constexpr complex_t(T real)          : real(real), imag(0.0f) {}
    inline constexpr complex_t(T real, T imag)  : real(real), imag(imag) {}

    inline constexpr complex_t operator+(const complex_t& other) {
        return complex_t(real + other.real, imag + other.imag);
    }

    inline constexpr complex_t operator+(const T& scalar) {
        return complex_t(real + scalar, imag);
    }

    inline constexpr complex_t operator-(const complex_t& other) {
        return complex_t(real - other.real, imag - other.imag);
    }

    inline constexpr complex_t operator-(const T& scalar) {
        return complex_t(real - scalar, imag);
    }

    inline constexpr complex_t operator*(const complex_t& other) {
        return complex_t(
            real * other.real - imag * other.imag,
            real * other.imag + imag * other.real
        );
    }

    inline constexpr complex_t operator/(const complex_t& other) {
        T val = other.real * other.real + other.imag * other.imag;

        return complex_t(
            (real * other.real + imag * other.imag) / val,
            (imag * other.real - real * other.imag) / val
        );
    }

    inline constexpr complex_t operator-() {
        return complex_t(-real, -imag);
    }

    inline constexpr complex_t operator==(const complex_t& other) {
        return real == other.real && imag == other.imag;
    }
};

void fft(complex_t<float>* data, int data_size, bool inverse);

class Filter2ndOrder
{
public:
    // [channel][order]
    float x[2][3]; // input
    float y[2][3]; // output

    // filter coefficients
    float a[2][3];
    float b[2][3];

    Filter2ndOrder();

    void low_pass(float Fs, float f0, float Q);
    void process(float input[2], float output[2]);
};