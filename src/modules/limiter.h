#pragma once
#include <atomic>
#include "../audio.h"
#include "../util.h"

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
        float _attack;
        float _decay;
        float _limit[2];
        float _in_volume[2];
        float _out_volume[2];

        static constexpr size_t BUFFER_SIZE = 1024;
        float _in_buf[2][BUFFER_SIZE];
        float _out_buf[2][BUFFER_SIZE];
        int _buf_i[2];
    public:
        void save_state(std::ostream& ostream) override;
        bool load_state(std::istream&, size_t size) override;

        LimiterModule(DestinationModule& dest);
    };
}