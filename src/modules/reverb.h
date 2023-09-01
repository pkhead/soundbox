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

        // keep two copies of the module state, one for the
        // processing thread, and another for the ui thread.
        struct module_state {
            float mix,
                  echo_delay,
                  feedback,
                  low_cut;
        } process_state, ui_state;
        MessageQueue state_queue;

        static constexpr float MAX_DELAY_LEN = 1.0f; // in seconds
        static constexpr size_t REVERB_CHANNEL_COUNT = 4;
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