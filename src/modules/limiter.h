#pragma once
#include <atomic>
#include "../audio.h"
#include "../util.h"

// TODO: multiple filters
namespace audiomod
{
    class LimiterModule : public ModuleBase {
    protected:
        void process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) override;
        void _interface_proc() override;
        
        // DO NOT DO ANY REALTIME-SAFE OPERATIONS
        // WHILE THE LOCK IS ACTIVE!!
        SpinLock lock;
        float _input_gain;
        float _output_gain;
        float _cutoff;
        float _ratio;
        float _attack;
        float _decay;
        float _envelope[2];
        float _buf[2][256];
        int _buf_i[2];

        float in_volume;
        float out_volume;

    public:
        void save_state(std::ostream& ostream) override;
        bool load_state(std::istream&, size_t size) override;

        LimiterModule(DestinationModule& dest);
    };
}