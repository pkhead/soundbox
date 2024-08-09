#include <catch2/catch_amalgamated.hpp>
#include <audio_engine/ring_buffer.hpp>
#include <modules/internal/dsp.h>

using namespace Catch;
using namespace hosts::internal;

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

/*
TEST_CASE("pad_align 1", "[utils]")
{
    REQUIRE(pad_align(sizeof(size_t) * 3 + 1, sizeof(size_t)) == sizeof(size_t) * 4);
}

TEST_CASE("pad_align 2", "[utils]")
{
    REQUIRE(pad_align(sizeof(size_t) * 4, sizeof(size_t)) == sizeof(size_t) * 4);
}
*/

// ringbuffer tests
TEST_CASE("RingBuffer write/read", "[ringbuffer]")
{
    int write_arr[7] = { 40, 72, 84, 12, 83, 45, 99 };
    int read_arr[7];
    bool success;

    SECTION("simple")
    {
        RingBuffer<int> buffer(6);

        SECTION("partial capacity")
        {
            success = buffer.write(write_arr, 4);
            REQUIRE(success == true);
            success = buffer.read(read_arr, 4);
            REQUIRE(success == true);

            REQUIRE(memcmp(write_arr, read_arr, 4 * sizeof(int)) == 0);
        }

        SECTION("full capacity")
        {
            success = buffer.write(write_arr, 6);
            REQUIRE(success == true);
            success = buffer.read(read_arr, 6);
            REQUIRE(success == true);

            REQUIRE(memcmp(write_arr, read_arr, 6 * sizeof(int)) == 0);
        }

        SECTION("exceeded capacity")
        {
            success = buffer.write(write_arr, 7);
            REQUIRE(success == false); 
        }
    }

    SECTION("overflow")
    {
        RingBuffer<int> buffer(5);

        // first write/read, no overflow
        buffer.write(write_arr, 4);
        buffer.read(read_arr, 4);

        success = buffer.write(write_arr + 4, 3);
        REQUIRE(success == true);
        success = buffer.read(read_arr, 3);
        REQUIRE(success == true);

        REQUIRE(memcmp(write_arr + 4, read_arr, 3 * sizeof(int)) == 0);
    }
}

TEST_CASE("RingBuffer space query", "[ringbuffer]")
{
    RingBuffer<int> buffer(7);
    REQUIRE(buffer.available_for_read() == 0);
    REQUIRE(buffer.available_for_write() == 7);

    int write_arr1[] = { 0, 1, 2 };
    buffer.write(write_arr1, 3);
    REQUIRE(buffer.available_for_read() == 3);
    REQUIRE(buffer.available_for_write() == 4);

    int write_arr2[] = { 11, 13, 14, 15 };
    buffer.write(write_arr2, 4);
    REQUIRE(buffer.available_for_read() == 7);
    REQUIRE(buffer.available_for_write() == 0);

    int read_arr[7];
    int require_arr[7] = { 0, 1, 2, 11, 13, 14, 15 };
    buffer.read(read_arr, 7);
    REQUIRE(buffer.available_for_read() == 0);
    REQUIRE(buffer.available_for_write() == 7);
    REQUIRE(memcmp(read_arr, require_arr, 7 * sizeof(int)) == 0);
}

/*TEST_CASE("RingBuffer test 1", "[ringbuffer]")
{
    RingBuffer<int> buffer(6);

    int arr1[4] = { 39, 21, 84, 23 };
    int arr2[4] = { 12, 95, 25, 92 };
    int read_buf[4];
    
    // write arr1
    REQUIRE(buffer.available_for_read() == 0);
    buffer.write(arr1, 4);
    REQUIRE(buffer.available_for_read() == 4);
    buffer.read(read_buf, 4);
    REQUIRE(memcmp(arr1, read_buf, 4 * sizeof(int)) == 0);
    REQUIRE(buffer.available_for_read() == 0);

    // write arr2
    // the size of the arrays were carefully chosen so
    // that it would overflow
    buffer.write(arr2, 4);
    REQUIRE(buffer.available_for_read() == 4);
    buffer.read(read_buf, 4);
    REQUIRE(memcmp(arr2, read_buf, 4 * sizeof(int)) == 0);
    REQUIRE(buffer.available_for_read() == 0);
}*/

TEST_CASE("RingBuffer test 2", "[ringbuffer]")
{
    RingBuffer<int> buffer(9);

    int arr1[6] = { 1, 2, 3, 4, 5, 6 };
    int arr2[4] = { 7, 8, 9, 10 };
    int arr3[3] = { 11, 12, 13 };
    int read_buf[6];

    REQUIRE(buffer.available_for_read() == 0);
    buffer.write(arr1, 6);
    REQUIRE(buffer.available_for_read() == 6);
    buffer.read(read_buf, 6);
    REQUIRE(memcmp(arr1, read_buf, 6 * sizeof(int)) == 0);
    REQUIRE(buffer.available_for_read() == 0);

    buffer.write(arr2, 4);
    buffer.write(arr3, 3);
    REQUIRE(buffer.available_for_read() == 7);
    buffer.read(read_buf, 4);
    REQUIRE(buffer.available_for_read() == 3);
    REQUIRE(memcmp(read_buf, arr2, 4 * sizeof(int)) == 0);
    buffer.read(read_buf, 3);
    REQUIRE(memcmp(read_buf, arr3, 3 * sizeof(int)) == 0);
    REQUIRE(buffer.available_for_read() == 0);
}

// not really a *unit* test
/*
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
*/