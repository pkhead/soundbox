#include <cstring>
#include <cmath>
#include <cassert>
#include <stdexcept>

#include "util.h"

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

        assert(!std::isinf(y[0]));

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

// thread-safe message queue
MessageQueue::MessageQueue(size_t data_capacity, size_t slot_count)
:   ringbuf((data_capacity + sizeof(size_t)) * slot_count)
{}

MessageQueue::~MessageQueue()
{}

template <typename T>
inline size_t pad_align(T a, T b)
{
    assert(b && ((b & (b - 1)) == 0));
    return (a + b-1) & -b;
}

int MessageQueue::post(const void* data, size_t size)
{
    if (size > ringbuf.writable()) return 1;

    size_t data_size = pad_align(sizeof(size_t) + size, sizeof(size_t));
    std::byte* msg_data = new std::byte[data_size];

    *((size_t*) msg_data) = size; // write message size
    memcpy(msg_data + sizeof(size_t), data, size); // write message data
    
    // write to ringbuffer
    ringbuf.write(msg_data, data_size);

    delete[] msg_data;
    return 0;
}

MessageQueue::read_handle_t::read_handle_t(RingBuffer<std::byte>& ringbuf)
    : _size(0), _buf(ringbuf), bytes_read(0)
{
    if (ringbuf.queued() > 0)
    {
        ringbuf.read((std::byte*) &_size, sizeof(size_t));
    }
}

MessageQueue::read_handle_t::read_handle_t(const read_handle_t&& src)
    : _size(src._size), _buf(src._buf), bytes_read(src.bytes_read)
{}

MessageQueue::read_handle_t::~read_handle_t()
{
    // when handle is closed, move read cursor to the next message
    _buf.read(nullptr, pad_align(_size, sizeof(size_t)) - bytes_read);
}

void MessageQueue::read_handle_t::read(void* out, size_t count)
{
    _buf.read((std::byte*) out, count);
    bytes_read += count;
}

MessageQueue::read_handle_t MessageQueue::read()
{
    return read_handle_t(this->ringbuf);
}



#ifdef UNIT_TESTS
#include <catch2/catch_amalgamated.hpp>
using namespace Catch;

TEST_CASE("Max 1", "[utils]") {
    REQUIRE(max(9, -3) == 9);
}

TEST_CASE("Max 2", "[utils]") {
    REQUIRE(max(10, 2) == 10);
}

TEST_CASE("Max 3", "[utils]") {
    REQUIRE(max(-8, -3) == -3);
}

TEST_CASE("Min 1", "[utils]") {
    REQUIRE(min(9, -3) == -3);
}

TEST_CASE("Min 2", "[utils]") {
    REQUIRE(min(10, 2) == 2);
}

TEST_CASE("Min 3", "[utils]") {
    REQUIRE(min(-8, -3) == -8);
}

TEST_CASE("Binary Sign 1", "[utils]") {
    REQUIRE(bsign(4.0f) == 1);
}

TEST_CASE("Binary Sign 2", "[utils]") {
    REQUIRE(bsign(-2.0f) == -1);
}

TEST_CASE("Binary Sign 3", "[utils]") {
    REQUIRE(bsign(0.0f) == 1);
}

TEST_CASE("Sign 1", "[utils]") {
    REQUIRE(sign(4.0f) == 1);
}

TEST_CASE("Sign 2", "[utils]") {
    REQUIRE(sign(-2.0f) == -1);
}

TEST_CASE("Sign 3", "[utils]") {
    REQUIRE(sign(0.0f) == 0);
}

TEST_CASE("Zero Crossing 1", "[utils]") {
    REQUIRE(is_zero_crossing(-1.0f, 1.0f));
}

TEST_CASE("Zero Crossing 2", "[utils]") {
    REQUIRE(is_zero_crossing(1.0f, -1.0f));
}

TEST_CASE("Zero Crossing 3", "[utils]") {
    REQUIRE(is_zero_crossing(0.0f, 0.0f));
}

TEST_CASE("Zero Crossing 4", "[utils]") {
    REQUIRE(is_zero_crossing(0.0f, 2.0f));
}

TEST_CASE("Zero Crossing 5", "[utils]") {
    REQUIRE(is_zero_crossing(-2.0f, 0.0));
}

TEST_CASE("Zero Crossing 6", "[utils]") {
    REQUIRE_FALSE(is_zero_crossing(2.0f, 1.0f));
}

TEST_CASE("Zero Crossing 7", "[utils]") {
    REQUIRE_FALSE(is_zero_crossing(-2.0f, -1.0f));
}

TEST_CASE("dB to factor 1", "[utils]") {
    REQUIRE_THAT(db_to_mult(3.0f), Matchers::WithinAbs(1.995262315f, 0.00001f));
}

TEST_CASE("dB to factor 2", "[utils]") {
    REQUIRE_THAT(db_to_mult(3.0), Matchers::WithinAbs(1.995262315, 0.00001));
}

TEST_CASE("pad_align 1", "[utils]")
{
    REQUIRE(pad_align(sizeof(size_t) * 3 + 1, sizeof(size_t)) == sizeof(size_t) * 4);
}

TEST_CASE("pad_align 2", "[utils]")
{
    REQUIRE(pad_align(sizeof(size_t) * 4, sizeof(size_t)) == sizeof(size_t) * 4);
}

// ringbuffer tests
// TODO: these are not *unit* tests, also does not test writable function
TEST_CASE("RingBuffer test 1", "[utils.ringbuffer]")
{
    RingBuffer<int> buffer(6);

    int arr1[4] = { 39, 21, 84, 23 };
    int arr2[4] = { 12, 95, 25, 92 };
    int read_buf[4];
    
    // write arr1
    REQUIRE(buffer.queued() == 0);
    buffer.write(arr1, 4);
    REQUIRE(buffer.queued() == 4);
    buffer.read(read_buf, 4);
    REQUIRE(memcmp(arr1, read_buf, 4 * sizeof(int)) == 0);
    REQUIRE(buffer.queued() == 0);

    // write arr2
    // the size of the arrays were carefully chosen so
    // that it would overflow
    buffer.write(arr2, 4);
    REQUIRE(buffer.queued() == 4);
    buffer.read(read_buf, 4);
    REQUIRE(memcmp(arr2, read_buf, 4 * sizeof(int)) == 0);
    REQUIRE(buffer.queued() == 0);
}

TEST_CASE("RingBuffer test 2", "[utils.ringbuffer]")
{
    RingBuffer<int> buffer(9);

    int arr1[6] = { 1, 2, 3, 4, 5, 6 };
    int arr2[4] = { 7, 8, 9, 10 };
    int arr3[3] = { 11, 12, 13 };
    int read_buf[6];

    REQUIRE(buffer.queued() == 0);
    buffer.write(arr1, 6);
    REQUIRE(buffer.queued() == 6);
    buffer.read(read_buf, 6);
    REQUIRE(memcmp(arr1, read_buf, 6 * sizeof(int)) == 0);
    REQUIRE(buffer.queued() == 0);

    buffer.write(arr2, 4);
    buffer.write(arr3, 3);
    REQUIRE(buffer.queued() == 7);
    buffer.read(read_buf, 4);
    REQUIRE(buffer.queued() == 3);
    REQUIRE(memcmp(read_buf, arr2, 4 * sizeof(int)) == 0);
    buffer.read(read_buf, 3);
    REQUIRE(memcmp(read_buf, arr3, 3 * sizeof(int)) == 0);
    REQUIRE(buffer.queued() == 0);
}

// not really a *unit* test
TEST_CASE("MessageQueue test", "[utils]")
{
    MessageQueue queue(90, 100);

    const std::string str1("ABISUDHUIEH");
    const std::string str2("8931uiHUIHIOJSD89");

    queue.post(str1.c_str(), str1.size());
    queue.post(str2.c_str(), str2.size());

    // string 1
    {
        char buf[str1.size() + 1];
        memset(buf, 0, sizeof(buf));

        auto handle = queue.read();
        REQUIRE(handle.size() == str1.size());
        handle.read(buf, str1.size());
        REQUIRE(std::string(buf) == str1);
    }

    // string 2
    {
        char buf[str2.size() + 1];
        memset(buf, 0, sizeof(buf));

        auto handle = queue.read();
        REQUIRE(handle.size() == str2.size());
        handle.read(buf, str2.size());
        REQUIRE(std::string(buf) == str2);
    }
}
#endif