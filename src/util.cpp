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

// Filters
Filter2ndOrder::Filter2ndOrder()
{
    for (int i = 0; i < 3; i++)
    {
        for (int c = 0; c < 2; c++)
        {
            x[c][i] = 0.0f;
            y[c][i] = 0.0f;
        }

        a[i] = 0.0f;
        b[i] = 0.0f;
    }
}

void Filter2ndOrder::process(float input[2], float output[2])
{
    for (int c = 0; c < 2; c++)
    {
        assert(!std::isinf(input[c]));

        x[c][0] = input[c];
        y[c][0] = b[0] * x[c][0] + b[1] * x[c][1] + b[2] * x[c][2]
                                 - a[1] * y[c][1] - a[2] * y[c][2];

        assert(!std::isinf(y[c][0]));

        x[c][2] = x[c][1];
        x[c][1] = x[c][0];
        y[c][2] = y[c][1];
        y[c][1] = y[c][0];

        output[c] = y[c][0];
    }
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

void Filter2ndOrder::peak(float Fs, float f0, float gain, float bw_scale)
{
    // Fs: sample rate
    // f0: frequency
    // gain: peak linear gain
    // bw: band width
    
    float w0 = 2.0 * M_PI * f0 / Fs;
    float sqrt_gain = sqrtf(exp10f(gain / 40.0f));
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
:   _slot_count(slot_count),
    _data_capacity(data_capacity)
{
    slots = new slot_t[_slot_count];
    
    for (size_t i = 0; i < _slot_count; i++) {
        slot_t& slot = slots[i];
        slot.size = 0;
        slot.ready = false;
        slot.reserved = false;
        slot.data = new uint8_t[_data_capacity];
    }
}

MessageQueue::~MessageQueue()
{
    for (size_t i = 0; i < _slot_count; i++) {
        slot_t& slot = slots[i];
        delete[] slot.data;
    }

    delete[] slots;
}

int MessageQueue::post(const void* data, size_t size)
{
    if (size > _data_capacity) return 1;

    for (size_t i = 0; i < _slot_count; i++)
    {
        slot_t& slot = slots[i];

        if (!slot.reserved.exchange(true))
        {
            slot.size = size;
            memcpy(slot.data, data, size);
            slot.ready = true;
            return 0;
        }
    }

    return 2;
}

MessageQueue::read_handle_t::read_handle_t(slot_t* slot)
    : _size(0), _data(nullptr), slot(slot)
{
    if (slot) {
        _size = slot->size;
        _data = slot->data;
    }
}

MessageQueue::read_handle_t::read_handle_t(const read_handle_t&& src)
{
    if (slot) slot->reserved = false;
    slot = src.slot;
    _data = src._data;
    _size = src._size;
}

MessageQueue::read_handle_t::~read_handle_t()
{
    if (slot) slot->reserved = false;
}

MessageQueue::read_handle_t MessageQueue::read()
{
    for (size_t i = 0; i < _slot_count; i++) {
        slot_t& slot = slots[i];
        
        if (slot.reserved && slot.ready.exchange(false))
            return read_handle_t(&slot);
    }

    return read_handle_t(nullptr);
}



#ifdef UNIT_TESTS
#include <catch2/catch_amalgamated.hpp>
using namespace Catch;

TEST_CASE("Max", "[utils]")
{
    REQUIRE(max(9, -3) == 9);
    REQUIRE(max(10, 2) == 10);
    REQUIRE(max(-8, -3) == -3);
}

TEST_CASE("Min", "[utils]")
{
    REQUIRE(min(9, -3) == -3);
    REQUIRE(min(10, 2) == 2);
    REQUIRE(min(-8, -3) == -8);
}

TEST_CASE("Binary Sign", "[utils]")
{
    REQUIRE(bsign(4.0f) == 1);
    REQUIRE(bsign(-2.0f) == -1);
    REQUIRE(bsign(0.0f) == 1);
}

TEST_CASE("Sign", "[utils]")
{
    REQUIRE(sign(4.0f) == 1);
    REQUIRE(sign(-2.0f) == -1);
    REQUIRE(sign(0.0f) == 0);
}

TEST_CASE("Zero Crossing", "[utils]")
{
    REQUIRE(is_zero_crossing(-1.0f, 1.0f));
    REQUIRE(is_zero_crossing(1.0f, -1.0f));
    REQUIRE(is_zero_crossing(0.0f, 0.0f));
    REQUIRE(is_zero_crossing(0.0f, 2.0f));
    REQUIRE(is_zero_crossing(-2.0f, 0.0));
    REQUIRE_FALSE(is_zero_crossing(2.0f, 1.0f));
    REQUIRE_FALSE(is_zero_crossing(-2.0f, -1.0f));
}

TEST_CASE("dB to factor", "[utils]")
{
    REQUIRE_THAT(db_to_mult(3.0f), Matchers::WithinAbs(1.995262315f, 0.00001f));
    REQUIRE_THAT(db_to_mult(3.0), Matchers::WithinAbs(1.995262315, 0.00001));
}

// ringbuffer tests
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
#endif