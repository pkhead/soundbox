#pragma once
#include "../audio.h"

namespace audiomod
{
    class GainModule : public ModuleBase {
    protected:
        void process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) override;
        void _interface_proc() override;
        
    public:
        float gain = 0.0f;

        void save_state(std::ostream& ostream) const override;
        bool load_state(std::istream& state, size_t size) override;

        GainModule(DestinationModule& dest);
    };
}