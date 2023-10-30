#pragma once

#include "../audio.h"
#include "../util.h"
#include "../dsp.h"
#include <ostream>

namespace audiomod {
    class WaveformSynth : public ModuleBase {
    protected:
        struct Voice {
            bool active = 0.0f;
            int key = 0.0f;
            float freq = 0.0f;
            float volume = 0.0f;
            double phase[3];
            float vibrato_phase = 0.0f;
            double time = 0.0f;

            ADSR::Instance amp_env;
            ADSR::Instance filt_env;
            
            double last_sample[3];
            Filter2ndOrder filter[2];

            Voice();
            Voice(int key, float freq, float volume);
        };

        enum WaveformType: uint8_t {
            Sine = (uint8_t)0,
            Square = (uint8_t)1,
            Sawtooth = (uint8_t)2,
            Triangle = (uint8_t)3,
            Pulse = (uint8_t)4,
            Noise = (uint8_t)5, // 1/4 pulse wave
        };

        enum FilterType: uint8_t {
            LowPassFilter = 0,
            HighPassFilter = 1,
            BandPassFilter = 2,
        };

        struct module_state_t
        {
            WaveformType waveform_types[3];
            float volume[3];
            float panning[3];
            int coarse[3];
            float fine[3];

            ADSR amp_env;
            
            FilterType filter_type;
            float filt_freq = 440.0f;
            float filt_reso = 0.0f;

            ADSR filt_env;
            float filt_amount = 0.0f;

            float vibrato_delay = 0.0f;
            float vibrato_speed = 1.0f;
            float vibrato_amount = 0.0f;
        } process_state, ui_state;

        MessageQueue event_queue, state_queue;

        // processing data
        static constexpr size_t MAX_VOICES = 16;
        Voice voices[MAX_VOICES];

        void process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) override;
        void _interface_proc() override;

        ModuleContext& modctx;
    public:
        WaveformSynth(ModuleContext& modctx);

        void event(const NoteEvent& event) override;
        void queue_event(const NoteEvent& event) override;
        void flush_events() override;
        void save_state(std::ostream& output) override;
        bool load_state(std::istream& input, size_t size) override;
    };
}