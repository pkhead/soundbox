#pragma once
#include <atomic>
#include "../audio.h"
#include "../dsp.h"

namespace audiomod
{
    class DelayModule : public ModuleBase {
    protected:
        void process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) override;
        void _interface_proc() override;

        // two copies of module state for each thread.
        // although, ui thread handles specifics about
        // time/tempo division, while processing thread
        // only cares about the delay time
        struct module_state_t
        {
            float delay_time[2];
            float feedback;
            float mix;
        } process_state;

        MessageQueue msg_queue;
        DelayLine<float> delay_line[2];

        // if the buffer was requested to be cleared
        std::atomic<bool> panic = false;
        
    public:
        // delay in seconds
        float _delay_time[2];
        int _tempo_division[2];
        bool _delay_mode;
        bool _lock;

        // feedback percentage
        float _feedback;
        // wet/dry mix
        float _mix;

        // the fields above have an underscore to
        // mitigate confusion between the fields and
        // local variables

        void save_state(std::ostream& ostream) override;
        bool load_state(std::istream&, size_t size) override;

        DelayModule(ModuleContext& modctx);
    };
}