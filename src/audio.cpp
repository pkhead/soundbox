#include <exception>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "audio.h"

static void write_callback(SoundIoOutStream* outstream, int frame_count_min, int frame_count_max) {
    AudioDevice* handle = (AudioDevice*)outstream->userdata;
    handle->write(outstream, frame_count_min, frame_count_max);
}

AudioDevice::AudioDevice(SoundIo* soundio, int output_device) {
    // if output_device == -1, use default output device
    if (output_device < 0) {
        output_device = soundio_default_output_device_index(soundio);

        if (output_device < 0) {
            fprintf(stderr, "no output device found");
            exit(1);
        }
    }

    // create output device
    device = soundio_get_output_device(soundio, output_device);
    if (!device) {
        fprintf(stderr, "out of memory!");
        exit(1);
    }

    // create output stream
    outstream = soundio_outstream_create(device);
    outstream->software_latency = 0.05;
    outstream->sample_rate = 48000;
    outstream->format = SoundIoFormatFloat32NE;
    outstream->userdata = this;
    outstream->write_callback = write_callback;

    // initialize output stream
    int err;

    if ((err = soundio_outstream_open(outstream))) {
        fprintf(stderr, "unable to open device: %s\n", soundio_strerror(err));
        exit(1);
    }

    if (outstream->layout_error)
        fprintf(stderr, "unable to set channel layout: %s\n", soundio_strerror(outstream->layout_error));

    // create ring buffer
    buf_capacity = (size_t)(outstream->software_latency * outstream->sample_rate) * 4;
    size_t capacity = buf_capacity * outstream->bytes_per_frame;
    ring_buffer = soundio_ring_buffer_create(soundio, capacity);
    buf_capacity = soundio_ring_buffer_capacity(ring_buffer);
    printf("buf_capacity: %li\n", buf_capacity);

    if ((err = soundio_outstream_start(outstream))) {
        fprintf(stderr, "unable to start device: %s\n", soundio_strerror(err));
        exit(1);
    }
}

AudioDevice::~AudioDevice() {
    soundio_outstream_destroy(outstream);
    soundio_device_unref(device);
}

bool AudioDevice::queue(const float* buffer, const size_t buffer_size) {
    if (buffer_size > (size_t)soundio_ring_buffer_free_count(ring_buffer)) return true;

    char* write_ptr = soundio_ring_buffer_write_ptr(ring_buffer);
    memcpy(write_ptr, buffer, buffer_size);
    soundio_ring_buffer_advance_write_ptr(ring_buffer, buffer_size);

    return false;
}

int AudioDevice::sample_rate() const {
    return outstream->sample_rate;
}

int AudioDevice::num_channels() const {
    return outstream->layout.channel_count;
}

int AudioDevice::num_queued_frames() const {
    size_t bytes_queued = buf_capacity - soundio_ring_buffer_free_count(ring_buffer);
    return bytes_queued / outstream->bytes_per_frame;
}

void AudioDevice::write(SoundIoOutStream* stream, int frame_count_min, int frame_count_max) {
    SoundIoChannelArea* areas;
    int frames_left, frame_count, err;

    char* read_ptr = soundio_ring_buffer_read_ptr(ring_buffer);
    int fill_bytes = soundio_ring_buffer_fill_count(ring_buffer);
    int fill_count = fill_bytes / stream->bytes_per_frame;

    if (fill_count < frame_count_min) {
        // ring buffer doesn't have enough data, fill with zeroes
        while (true) {
            frames_left = frame_count_min;

            if (frame_count_min <= 0) return;
            if ((err = soundio_outstream_begin_write(stream, &areas, &frame_count))) {
                fprintf(stderr, "begin write error: %s\n", soundio_strerror(err));
                return;
            }
            if (frame_count <= 0) return;

            for (int frame = 0; frame < frame_count; frame++) {
                for (int ch = 0; ch < stream->layout.channel_count; ch++) {
                    memset(areas[ch].ptr, 0, stream->bytes_per_sample);
                    areas[ch].ptr += areas[ch].step;
                }
            }

            if ((err = soundio_outstream_end_write(stream))) {
                fprintf(stderr, "end write error: %s\n", soundio_strerror(err));
                return;
            }
        }
    }

    int read_count = frame_count_max < fill_count ? frame_count_max : fill_count;
    frames_left = read_count;

    while (frames_left > 0) {
        frame_count = frames_left;
        if ((err = soundio_outstream_begin_write(stream, &areas, &frame_count))) {
            fprintf(stderr, "begin write error: %s\n", soundio_strerror(err));
            return;
        }
        if (frame_count <= 0) break;

        for (int frame = 0; frame < frame_count; frame++) {
            for (int ch = 0; ch < stream->layout.channel_count; ch++) {
                memcpy(areas[ch].ptr, read_ptr, stream->bytes_per_sample);
                areas[ch].ptr += areas[ch].step;
                read_ptr += stream->bytes_per_sample;
            }
        }

        if ((err = soundio_outstream_end_write(stream))) {
            fprintf(stderr, "end write error: %s\n", soundio_strerror(err));
            return;
        }

        frames_left -= frame_count;
    }

    soundio_ring_buffer_advance_read_ptr(ring_buffer, read_count * stream->bytes_per_frame);
}