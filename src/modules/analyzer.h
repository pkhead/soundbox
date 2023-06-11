#pragma once
#include "../audio.h"

namespace audiomod
{
    class AnalyzerModule : public ModuleBase {
    protected:
        void process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) override;
        void _interface_proc() override;

        // 0 = view samples
        // 1 = view spectrogram
        int mode = 0;
        float range = 1.0f;

        size_t buf_size = 0;

        // use double buffering because of concurrency
        std::atomic<int> front_buf = 0;
        std::atomic<bool> ready = false;
        std::atomic<bool> in_use = false; // if front buf is in use
        float* left_channel[2] = { nullptr, nullptr };
        float* right_channel[2] = { nullptr, nullptr };

    public:
        AnalyzerModule(Song* song);
        ~AnalyzerModule();
    };
}