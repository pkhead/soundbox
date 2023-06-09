#include "audiofile.h"
#include "sys.h"
#include <cstdint>

namespace audiofile {
    WavWriter::WavWriter(std::ostream& stream, size_t total_frames, uint16_t channels, uint32_t sample_rate) :
    _channels(channels),
    stream(stream),
    total_samples(total_frames * channels),
    written_samples(0)
    {
        // TODO: 24-bit samples?
        const uint32_t BYTES_PER_SAMPLE = sizeof(uint16_t);
        uint32_t num_samples = total_frames * channels;
        uint32_t chunk_size = num_samples * BYTES_PER_SAMPLE;

        // write wav header
        stream << "RIFF";
        push_bytes(stream, (uint32_t)(36 + chunk_size));
        stream << "WAVEfmt ";
        push_bytes(stream, (uint32_t)16); // pcm mode
        push_bytes(stream, (uint16_t)1); // audio format
        push_bytes(stream, channels); // number of channels
        push_bytes(stream, sample_rate); // sample rate
        push_bytes(stream, (uint32_t)(sample_rate * channels * BYTES_PER_SAMPLE)); // byte rate
        push_bytes(stream, (uint16_t)(channels * BYTES_PER_SAMPLE)); // block align
        push_bytes(stream, (uint16_t)(BYTES_PER_SAMPLE * (uint16_t)8)); // bits per sample

        // write beginning of wav data
        stream << "data";
        push_bytes(stream, chunk_size); // chunk size
    }

    void WavWriter::write_block(float* data, size_t size) {
        for (size_t i = 0; i < size; i++) {
            if (written_samples >= total_samples) break;

            float float_smp = data[i];
            if (float_smp > 1.0f) float_smp = 1.0f;
            if (float_smp < -1.0f) float_smp = -1.0f;

            //float int_smp = (float_smp + 1.0f) / 2.0f * UINT16_MAX;
            float int_smp = float_smp * INT16_MAX;
            push_bytes(stream, (int16_t)int_smp);

            written_samples++;
        }
    }
}