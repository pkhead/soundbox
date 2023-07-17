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

        // todo: atomic floats are likely to not be lock free
        std::atomic<float> frequency[2];
        std::atomic<float> resonance[2];

        // peaking filters
        static constexpr int NUM_PEAKS = 8;
        Filter2ndOrder peak_filter[NUM_PEAKS];
        std::atomic<float> peak_frequency[NUM_PEAKS];
        std::atomic<float> peak_resonance[NUM_PEAKS];
        std::atomic<bool>  peak_enabled  [NUM_PEAKS];

    public:
        void save_state(std::ostream& ostream) const override;
        bool load_state(std::istream&, size_t size) override;

        EQModule(DestinationModule& dest);
    };
}