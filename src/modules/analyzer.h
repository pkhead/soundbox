#pragma once
#include "../audio.h"
#include "../util.h"

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

        // 0 = view oscilloscope
        // 1 = view spectum
        int mode = 0;
        bool align = true; // oscilloscope align
        float range = 1.0f;

        // use double buffering because of concurrency
        std::atomic<int> window_front = 0;
        std::atomic<bool> ready = false;
        std::atomic<bool> in_use = false; // if front buf is in use

        // oscilloscope windows
        float* window_left[2] = { nullptr, nullptr };
        float* window_right[2] = { nullptr, nullptr };

        // buffers, used by ui thread
        // to do FFT and stuff for spectrum display
        complex_t<float> *complex_left, *complex_right;
        float            *real_left,    *real_right;
    public:
        AnalyzerModule(DestinationModule& dest);
        ~AnalyzerModule();
    };
}