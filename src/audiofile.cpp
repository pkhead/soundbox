#include "audiofile.h"
#include "sys.h"
#include <cstdint>

namespace audiofile {
    void write_wav(std::ostream& stream, float* audio_data, size_t data_size, uint16_t channels, uint32_t sample_rate) {
        // TODO: 24-bit samples?
        const uint32_t BYTES_PER_SAMPLE = sizeof(uint16_t);
        uint32_t num_samples = data_size / sizeof(float) / channels;
        uint32_t chunk_size = num_samples * channels * BYTES_PER_SAMPLE;

        // write wav header
        stream << "RIFF";
        push_bytes(stream, (uint32_t)36 + chunk_size);
        stream << "WAVEfmt ";
        push_bytes(stream, (uint32_t)16); // pcm mode
        push_bytes(stream, (uint16_t)1); // audio format
        push_bytes(stream, channels); // number of channels
        push_bytes(stream, sample_rate); // sample rate
        push_bytes(stream, (uint32_t)sample_rate * channels * BYTES_PER_SAMPLE); // byte rate
        push_bytes(stream, (uint16_t)channels * BYTES_PER_SAMPLE); // block align
        push_bytes(stream, (uint16_t)BYTES_PER_SAMPLE * 8); // bits per sample

        // write wav data
        stream << "data";
        push_bytes(stream, chunk_size); // chunk size

        for (size_t i = 0; i < data_size; i++) {
            float float_smp = audio_data[i];
            if (float_smp > 1.0f) float_smp = 1.0f;
            if (float_smp < -1.0f) float_smp = -1.0f;

            uint16_t int_smp = (float_smp + 1.0f) / 2.0f * UINT16_MAX;
            push_bytes(stream, int_smp);
        }
    }
}