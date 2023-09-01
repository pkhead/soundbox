#pragma once
#include "../audio.h"
#include "../util.h"

namespace audiomod
{
    class ReverbModule : public ModuleBase
    {
    protected:
        void process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) override;
        void _interface_proc() override;

        static constexpr float MAX_DELAY_LEN = 2.0f; // in seconds
        static constexpr size_t REVERB_CHANNEL_COUNT = 8;
        float mix_mult;

        size_t delay_len[8], delay_index[8]; // these are in samples
        float* delay_bufs[REVERB_CHANNEL_COUNT];

    public:
        ReverbModule(DestinationModule& dest);
        ~ReverbModule();
    };
}