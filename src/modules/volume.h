#pragma once
#include "../audio.h"

namespace audiomod
{
    class VolumeModule : public ModuleBase {
    protected:
        void process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) override;
        float cur_volume[2];
        float last_sample[2];
    
    public:
        float volume = 0.5f;
        float panning = 0.0f;
        
        bool mute = false;

        // used for soloing
        bool mute_override = false;

        VolumeModule();
        void save_state(std::ostream& ostream) const override;
        bool load_state(std::istream&, size_t size) override;
    };
}