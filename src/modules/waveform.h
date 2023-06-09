#pragma once

#include "../audio.h"
#include <ostream>

namespace audiomod {
    class WaveformSynth : public ModuleBase {
    protected:
        struct Voice {
            bool active;
            int key;
            float freq;
            float volume;
            double phase[3];
            double time;
            double release_time;
            float release_env;
            double last_sample[3];
        };

        static constexpr size_t MAX_VOICES = 16;
        Voice voices[MAX_VOICES];
        void process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) override;
        void _interface_proc() override;
    public:
        WaveformSynth(DestinationModule& dest);

        enum WaveformType: uint8_t {
            Sine = (uint8_t)0,
            Square = (uint8_t)1,
            Sawtooth = (uint8_t)2,
            Triangle = (uint8_t)3,
            Pulse = (uint8_t)4,
            Noise = (uint8_t)5, // 1/4 pulse wave
        };
        
        WaveformType waveform_types[3];
        float volume[3];
        float panning[3];
        int coarse[3];
        float fine[3];

        float attack = 0.0f;
        float decay = 0.0f;
        float sustain = 1.0f;
        float release = 0.0f;

        void event(const MidiEvent& event) override;
        void save_state(std::ostream& output) const override;
        bool load_state(std::istream& input, size_t size) override;
    };
}