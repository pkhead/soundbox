#pragma once
#include <stddef.h>
#include <ostream>

namespace audiofile {
    class WavWriter {
    private:
        std::ostream& stream;

    public:
        WavWriter(std::ostream& stream, size_t total_frames, uint16_t channels, uint32_t sample_rate);

        // Write a block of audio data to the stream
        void write_block(float* data, size_t size);
    };
}