#pragma once

#ifdef _WIN32
// Windows
#include <windows.h>

#define sleep(time) Sleep((time) * 1000)

// prevent collision with user-defined max and min function
#undef max
#undef min

#else
// Unix
#include <unistd.h>

#define sleep(time) usleep((time) * 1000000)

#endif

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