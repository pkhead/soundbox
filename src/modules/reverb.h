#pragma once
#include "../audio.h"
#include "../util.h"
#include "../dsp.h"

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
                  diffuse,
                  echo_delay,
                  feedback,
                  shelf_freq,
                  shelf_gain;
        } process_state, ui_state;
        MessageQueue state_queue;

        static constexpr float MAX_DELAY_LEN = 1.2f; // in seconds
        static constexpr size_t REVERB_CHANNEL_COUNT = 8;
        static constexpr size_t DIFFUSE_STEPS = 2;
        Filter2ndOrder shelf_filters[REVERB_CHANNEL_COUNT];

        // this allocates >10 mb of memory lol
        DelayLine<float>
            diffuse_delays[DIFFUSE_STEPS][REVERB_CHANNEL_COUNT],
            echoes[REVERB_CHANNEL_COUNT];
            //early_delays[REVERB_CHANNEL_COUNT];
        
        float diffuse_factors[DIFFUSE_STEPS][REVERB_CHANNEL_COUNT];
        float diffuse_delay_mod[DIFFUSE_STEPS][REVERB_CHANNEL_COUNT];

        void diffuse(int index, float* values);

        ModuleContext& modctx;

    public:
        ReverbModule(ModuleContext& dest);
        ~ReverbModule();

        void save_state(std::ostream& ostream) override;
        bool load_state(std::istream&, size_t size) override;
    };
}