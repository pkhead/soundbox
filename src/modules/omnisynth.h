#pragma once

#include "../audio.h"
#include "../util.h"
#include "../dsp.h"

/**
* Heavily inspired by the Synth One app from the App Store
**/

namespace audiomod {
    class OmniSynth : public ModuleBase {
    protected:
        enum LFOShape: uint8_t {
            Sine = (uint8_t)0,
            Square = (uint8_t)1,
            Rise = (uint8_t)2,
            Fall = (uint8_t)3,
        };

        enum FilterType: uint8_t {
            LowPass = (uint8_t)0,
            HighPass = (uint8_t)1,
            BandPass = (uint8_t)2
        };

        enum ModulatorTarget: uint8_t {
            Cutoff = (uint8_t)0,
            Reso,
            OscMix,
            Reverb,
            Decay,
            Noise,
            FM1, FM2,
            FilterEnv,
            Pitch,
            Bitcrush
        };

        enum ArpeggioDirection: uint8_t {
            Up,
            UpDown,
            Down
        };

        static constexpr int FX_MOD_TARGET_COUNT = 11;
        static constexpr int SEQUENCER_STEP_COUNT = 16; 

        struct module_state_t
        {
            // from main tab
            float global_volume;

            float osc_shapes[2];
            int osc_coarse[2];
            float osc_fine[2];

            float osc_volume[2];
            float osc_mix;

            float fm_vol;
            float fm_mod;

            float sub_volume;
            bool sub_lower;  // sub oscillator is -24st instead of -12st
            bool sub_square; // sub oscillator is a square?

            FilterType filter_type;
            float filter_freq;
            float filter_reso;

            float noise_volume;
            float glide;
            bool mono;

            // from env tab
            ADSR amp_env;
            ADSR filt_env;
            ADSR fx_env;

            // from FX tab
            LFOShape lfo_shape[2];
            float lfo_rate[2];
            float lfo_amp[2];

            bool fx_target_lfo1[FX_MOD_TARGET_COUNT];
            bool fx_target_lfo2[FX_MOD_TARGET_COUNT];
            bool fx_target_env[FX_MOD_TARGET_COUNT];

            float bitcrush;
            float reverb_size;
            float reverb_lowcut;
            float reverb_mix;
            float delay_time;
            float delay_feedback;
            float delay_mix;

            float autopan_rate;
            float autopan_amt;

            float phaser_rate;
            float phaser_mix;
            float phaser_taps;
            float phaser_notch;

            // from sequencer tab
            bool enable_seq;
            bool arp_or_seq; // use arp is false, use seq if true
            int seq_steps;
            float seq_division;

            // arpeggiator options
            int arp_octave;
            ArpeggioDirection arp_dir;

            // sequencer options
            int seq_semitone[SEQUENCER_STEP_COUNT];
            bool seq_note_on[SEQUENCER_STEP_COUNT];

            module_state_t();
        } process_state, ui_state;

        static void state_init(module_state_t& state);

        struct Voice {
            bool active = 0.0f;
            int key = 0.0f;
            float freq = 0.0f;
            float volume = 0.0f;
            double phase[3];
            double time = 0.0f;

            ADSR::Instance amp_env;
            ADSR::Instance filt_env;
            ADSR::Instance mod_env;

            double last_sample[3];
            Filter2ndOrder filter[2];

            Voice();
            Voice(int key, float freq, float volume);
        };

        static constexpr size_t MAX_VOICES = 16;
        Voice voices[MAX_VOICES];
        
        MessageQueue event_queue;
        MessageQueue state_queue;

        void process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) override;
        void _interface_proc() override;

        ModuleContext& modctx;

    private:
        void tab_main();
        void tab_env();
        void tab_fx();
        void tab_seq();

    public:
        OmniSynth(ModuleContext& modctx);

        void event(const NoteEvent& event) override;
        void queue_event(const NoteEvent& event) override;
        void save_state(std::ostream& output) override;
        bool load_state(std::istream& input, size_t size) override;
    }; // class OmniSynth
} // namespace audiomod