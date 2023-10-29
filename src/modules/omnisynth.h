#pragma once

/**
* Essentially GoldBox advanced fm
**/

#include "../audio.h"
#include "../util.h"

namespace audiomod {
    class OmniSynth : public ModuleBase {
    protected:
        enum WaveformType: uint8_t {
            Sine = (uint8_t)0,
            Square = (uint8_t)1,
            Sawtooth = (uint8_t)2,
            Triangle = (uint8_t)3,
            Pulse = (uint8_t)4,
        };

        enum FilterType: uint8_t {
            LowPass = (uint8_t)0,
            HighPass = (uint8_t)0,
            BandPass = (uint8_t)0
        };
        
        WaveformType waveform_types[6];
        float volume[6];
        float freq[6];

        float attack = 0.0f;
        float decay = 0.0f;
        float sustain = 1.0f;
        float release = 0.0f;

        struct adsr_t
        {
            float attack, decay, sustain, release;
        };

        struct module_state_t
        {
            WaveformType waveforms[6];
            float volume[6];
            float freq[6];

            adsr_t amp_envelope;
            adsr_t filter_envelope;
            float filter_envelope_amt;

            FilterType filter_type;
            float filter_freq;
            float filter_reso;
        } process_state, ui_state;

        struct Voice {
            bool active;
            
            int key;
            float freq;
            float volume;
            double time;
            double release_time;
            float release_env;

            // for each oscillator
            double phase[6];
            double last_sample[6];
        };

        static constexpr size_t MAX_VOICES = 16;
        Voice voices[MAX_VOICES];
        
        MessageQueue event_queue;
        MessageQueue state_queue;

        void process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) override;
        void _interface_proc() override;

        ModuleContext& modctx;
    public:
        OmniSynth(ModuleContext& modctx);

        void event(const NoteEvent& event) override;
        void queue_event(const NoteEvent& event) override;
        void flush_events() override;
        void save_state(std::ostream& output) override;
        bool load_state(std::istream& input, size_t size) override;
    };
}