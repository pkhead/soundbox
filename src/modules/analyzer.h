#pragma once
#include "../audio.h"
#include "util.h"

namespace audiomod
{
    class AnalyzerModule : public ModuleBase {
    protected:
        void process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) override;
        void _interface_proc() override;

        RingBuffer ring_buffer;

        const int frames_per_window = 1024;
        const int window_margin = 512; // in frames
        float* buf;

        // 0 = view samples
        // 1 = view spectrogram
        int mode = 0;
        float range = 1.0f;

        // use double buffering because of concurrency
        std::atomic<int> window_front = 0;
        std::atomic<bool> ready = false;
        std::atomic<bool> in_use = false; // if front buf is in use
        float* window_left[2] = { nullptr, nullptr };
        float* window_right[2] = { nullptr, nullptr };

    public:
        AnalyzerModule(DestinationModule& dest);
        ~AnalyzerModule();
    };
}