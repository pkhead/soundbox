#pragma once

#ifdef _WIN32
// Windows
#include <windows.h>

#define sleep(time) Sleep((time) * 1000)

// prevent collision with user-defined max and min function
#undef max
#undef min

#else
// Linux/Unix
#include <unistd.h>

#define sleep(time) usleep((time) * 1000000)

#endif

#include <ctime>
#include <stdint.h>
#include <ostream>
#include <istream>

inline const clock_t elapsed_start() { return clock(); };
const double elapsed(const clock_t& since);

extern bool IS_BIG_ENDIAN;

// procedure to convert between native endian and little endian
template <typename T>
T swap_little_endian(const T& value) {
    if (IS_BIG_ENDIAN) {
        uint8_t* bytes = (uint8_t*)&value;
        uint8_t output_bytes[sizeof(value)];

        for (size_t i = 0; i < sizeof(value); i++) {
            output_bytes[i] = bytes[sizeof(value) - i - 1]; 
        }

        return *((T*)(output_bytes));
    } else {
        return value;       
    }
}

// push the bytes of data in little-endian order
template <typename T>
static void push_bytes(std::ostream& out, T data) {
    if (IS_BIG_ENDIAN) {
        for (size_t i = sizeof(data) - 1; i > 0; i--) {
            out << ((uint8_t*)(&data)) [i];
        }
        out << ((uint8_t*)(&data)) [0];
    } else {
        for (size_t i = 0; i < sizeof(data); i++) {
            out << ((uint8_t*)(&data)) [i];
        }
    }
}

// retrieve bytes from an input stream in little-endian order
template <typename T>
static void pull_bytes(std::istream& in, T& output) {
    char next_char;
    uint8_t bytes[sizeof(T)];

    if (IS_BIG_ENDIAN) {
        for (size_t i = sizeof(T) - 1; i > 0; i--) {
            in.get(next_char);
            bytes[i] = next_char;
        }

        in.get(next_char);
        bytes[0] = next_char;
    } else {
        for (size_t i = 0; i < sizeof(T); i++) {
            in.get(next_char);
            bytes[i] = next_char;
        }
    }

    output = *((T*)bytes);
}