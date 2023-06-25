#pragma once
#include <cstddef>
#include <cstdint>
#include <ostream>

namespace audiofile {
    class WavWriter {
    private:
        std::ostream& stream;

        int _channels;

    public:
        WavWriter(std::ostream& stream, size_t total_frames, uint16_t channels, uint32_t sample_rate);

        size_t total_samples;
        size_t written_samples;

        // Write a block of audio data to the stream
        void write_block(float* data, size_t size);
    };
}