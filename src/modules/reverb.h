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
        static constexpr size_t DIFFUSE_STEPS = 4;
        float mix_mult;

        DelayLine<float> diffuse_delays[DIFFUSE_STEPS][REVERB_CHANNEL_COUNT], echoes[REVERB_CHANNEL_COUNT];
        float diffuse_factors[DIFFUSE_STEPS][REVERB_CHANNEL_COUNT];

        void diffuse(int index, float* values);

    public:
        ReverbModule(DestinationModule& dest);
        ~ReverbModule();
    };
}