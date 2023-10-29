#include <cstring>
#include <cmath>
#include <cassert>
#include <stdexcept>

#include "util.h"

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
    size_t data_size = pad_align(sizeof(size_t) + size, sizeof(size_t));
    if (data_size > ringbuf.writable()) return 1;
    
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