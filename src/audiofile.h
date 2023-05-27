#pragma once
#include <stddef.h>
#include <ostream>

namespace audiofile {
    void write_wav(std::ostream& stream, float* audio_data, size_t audio_size, int channels, int sample_rate);
}