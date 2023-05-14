#pragma once
#include <soundio.h>

class AudioDevice {
private:
    SoundIoDevice* device;
    SoundIoOutStream* outstream;
    SoundIoRingBuffer* ring_buffer;
    size_t buf_capacity;

public:
    AudioDevice(SoundIo* soundio, int output_device);
    ~AudioDevice();

    int sample_rate() const;
    int num_channels() const;

    bool queue(const float* buffer, const size_t buffer_size);
    int num_queued_frames() const;

    void write(SoundIoOutStream* stream, int frame_count_min, int frame_count_max);
};