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
        
        Filter2ndOrder filter;

        enum FilterMode : uint8_t
        {
            LowPassFilter = 0,
            HighPassFilter = 1,
            BandPassFilter = 2
        };

        std::atomic<FilterMode> filter_mode;

        // todo: atomic floats are likely to not be lock free
        std::atomic<float> frequency;
        std::atomic<float> resonance;

    public:
        void save_state(std::ostream& ostream) const override;
        bool load_state(std::istream&, size_t size) override;

        EQModule(DestinationModule& dest);
    };
}