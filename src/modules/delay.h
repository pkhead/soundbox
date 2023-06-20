#pragma once
#include <atomic>
#include "../audio.h"

namespace audiomod
{
    class DelayModule : public ModuleBase {
    protected:
        void process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) override;
        void _interface_proc() override;

        // TODO: size buffer based off sample rate (requires passing it in in constructor, or detecting a change)
        size_t delay_line_size;
        size_t delay_line_index[2];
        float* delay_line[2];

        // if the buffer was requested to be cleared
        std::atomic<bool> panic = false;
        
    public:
        // delay in seconds
        std::atomic<float> delay_time;
        std::atomic<int> tempo_division;
        std::atomic<bool> delay_mode;
        std::atomic<float> stereo_offset;

        // feedback percentage
        std::atomic<float> feedback;
        // wet/dry mix
        std::atomic<float> mix;

        void save_state(std::ostream& ostream) const override;
        bool load_state(std::istream&, size_t size) override;

        DelayModule(Song* song);
        ~DelayModule();
    };
}