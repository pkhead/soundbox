#pragma once
#include <cstddef>
#include <cmath>
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

// binary sign -- returns only two results
template <typename T>
inline int bsign(T v) {
    return v >= 0 ? 1 : -1; 
}

template <typename T>
inline int sign(T v) {
    return v == 0 ? 0 : bsign(v);
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

inline float db_to_mult(float db) {
    return powf(10.0f, db / 10.0f);
}

inline double db_to_mult(double db) {
    return powf(10.0f, db / 10.0);
}

size_t convert_from_stereo(float* src, float** dest, size_t channel_count, size_t frames_per_buffer, bool interleave);
void convert_to_stereo(float** src, float* dest, size_t channel_count, size_t frames_per_buffer, bool interleave);

// a RingBuffer
template <class T = float>
class RingBuffer
{
private:
    size_t audio_buffer_capacity = 0;
    T* audio_buffer = nullptr;
    std::atomic<size_t> buffer_write_ptr = 0;
    std::atomic<size_t> buffer_read_ptr = 0;

public:
    RingBuffer(size_t capacity)
    {
        audio_buffer_capacity = capacity;
        audio_buffer = new T[audio_buffer_capacity];
        buffer_write_ptr = 0;
        buffer_read_ptr = 0;
    }

    ~RingBuffer()
    {
        delete[] audio_buffer;
    }
    
    size_t queued() const
    {
        size_t write_ptr = buffer_write_ptr;
        size_t read_ptr = buffer_read_ptr;

        if (write_ptr >= read_ptr) {
            return write_ptr - read_ptr;
        } else {
            return audio_buffer_capacity - read_ptr + write_ptr;
        }
    }

    void write(T* buf, size_t size)
    {
        size_t write_ptr = buffer_write_ptr;

        if (write_ptr + size > audio_buffer_capacity)
        {
            // copy two chunks of the buffer to the right places
            // this will not work if it ends up writing the entire buffer and
            // still needs to wrap (if size and write_ptr are very big)
            // but nowhere in this code will that happen
            size_t split1_size = audio_buffer_capacity - write_ptr;
            memcpy(audio_buffer + write_ptr, buf, split1_size * sizeof(T));
            memcpy(audio_buffer, buf + split1_size, (size - split1_size) * sizeof(T));
        }
        else // no bounds in sight
        {
            memcpy(audio_buffer + write_ptr, buf, size * sizeof(T));
        }

        buffer_write_ptr = (write_ptr + size) % audio_buffer_capacity;
}

    size_t read(T* out, size_t size)
    {
        size_t num_read = 0;
        size_t write_ptr = buffer_write_ptr;
        size_t read_ptr = buffer_read_ptr;

        for (size_t i = 0; i < size; i++)
        {
            // break if there is no data more in buffer
            if (read_ptr == write_ptr)
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
};

//////////////////////////////////
// data type for complex number //
//////////////////////////////////
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

///////////////////
// FFT algorithm // (TODO: need more efficient fft)
///////////////////
void fft(complex_t<float>* data, int data_size, bool inverse);

// 2nd-order IIR filters
class Filter2ndOrder
{
public:
    // [channel][order]
    float x[2][3]; // input
    float y[2][3]; // output

    // filter coefficients
    float a[3];
    float b[3];

    Filter2ndOrder();

    void low_pass(float sample_rate, float frequency, float linear_gain);
    void high_pass(float sample_rate, float frequency, float linear_gain);
    void all_pass(float sample_rate, float frequency, float linear_gain);
    void peak(float sample_rate, float frequency, float linear_gain, float bandwidth);

    void process(float input[2], float output[2]);
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

    inline float read() {
        return buf[index];
    }

    inline void write(float v) {
        buf[index++] = v;
        if (index > delay) index = 0;
    }

    inline size_t max_size() const { return _max_size; }
};

// thread-safe message queue
class MessageQueue
{
private:
    size_t _data_capacity;
    size_t _slot_count;

    struct slot_t {
        std::atomic<bool> reserved;
        std::atomic<bool> ready;
        size_t size;
        uint8_t* data;
    };

    slot_t* slots;
public:
    class read_handle_t {
    private:
        slot_t* slot;
        size_t _size;
        const uint8_t* _data;

    public:
        read_handle_t(const read_handle_t& src) = delete;
        read_handle_t(const read_handle_t&& src);
        read_handle_t(slot_t* slot = nullptr);
        ~read_handle_t();

        inline size_t size() const { return _size; }
        inline const uint8_t* data() const { return _data; };
        inline operator bool() { return slot != nullptr; };
    };

    MessageQueue(size_t data_capacity, size_t total_slots);
    ~MessageQueue();

    // Post a message to the queue. Returns 0 on success. If
    // the data size was too large, it returns 1. If the
    // queue is full, it returns 2
    int post(const void* data, size_t size);

    // Get a read handle to a message. Once
    // the read handle is done being used, the message
    // will be freed from the queue.
    read_handle_t read();
};


class SpinLock {
private:
    std::atomic<bool> _lock = {0};

public:
    void lock() noexcept
    {
        for (;;) {
            if (!_lock.exchange(true, std::memory_order_acquire))
                return;

            while (_lock.load(std::memory_order_relaxed))
                __builtin_ia32_pause(); // i think msvc uses a different function
        }
    }

    void unlock() noexcept
    {
        _lock.store(false, std::memory_order_release);
    }
    
    class spinlock_guard {
    private:
        SpinLock& self;
    public:
        inline spinlock_guard(SpinLock& self) : self(self) {
            self.lock();
        };
        
        inline ~spinlock_guard() {
            self.unlock();
        };
    };
    
    inline spinlock_guard lock_guard() {
        return spinlock_guard(*this);
    };
};
