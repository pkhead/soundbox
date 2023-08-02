#pragma once
#include <atomic>
#include "../audio.h"
#include "../util.h"

// TODO: multiple filters
namespace audiomod
{
    class EQModule : public ModuleBase {
    protected:
        void process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) override;
        void _interface_proc() override;
        
        // low-pass & high-pass filters
        Filter2ndOrder filter[2];

        // peaking filters
        static constexpr int NUM_PEAKS = 8;
        Filter2ndOrder peak_filter[NUM_PEAKS];

        // keep two copies of the module state, one for the
        // processing thread, and another for the ui thread.
        // the ui thread will send state updates to the
        // processing thread
        struct module_state {
            float frequency[2],
                  resonance[2],
                  peak_frequency[NUM_PEAKS],
                  peak_resonance[NUM_PEAKS];
            bool peak_enabled[NUM_PEAKS];
        } process_state, ui_state;

        MessageQueue queue;
        std::atomic_flag sent_state;
    public:
        void save_state(std::ostream& ostream) override;
        bool load_state(std::istream&, size_t size) override;

        EQModule(DestinationModule& dest);
    };
}