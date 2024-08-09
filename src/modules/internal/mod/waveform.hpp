#pragma once
#include <modules/modules.hpp>
#include <dsp.hpp>

namespace hosts::internal
{
    ////////////////////
    // sbox::waveform //
    ////////////////////
    class WaveformModule : public modx::ModuleBase
    {
    private:
        static constexpr unsigned int MAX_VOICES = 16;
        static constexpr unsigned int OSC_COUNT = 3;
        
        struct Voice {
            bool active = 0.0f;
            int key = 0.0f;
            float freq = 0.0f;
            float volume = 0.0f;
            double phase[3];
            float vibrato_phase = 0.0f;
            double time = 0.0f;

            dsp::ADSR::Instance amp_env;
            dsp::ADSR::Instance filt_env;
            
            double last_sample[3];
            dsp::FilterIIR2ndOrder filter[2];

            Voice();
            Voice(int key, float freq, float volume);
        };

        struct
        {
            Voice voices[MAX_VOICES];
            dsp::ADSR amp_env;
            dsp::ADSR filt_env;
        } audio_state;

        int ui_selected_osc;

        void event(const modx::TrackEvent &ev);
    public:
        static constexpr unsigned int OSC_CONTROL_COUNT = 5;
        const unsigned int OSC_CONTROL_START[OSC_CONTROL_COUNT] = {
            0 * OSC_CONTROL_COUNT,
            1 * OSC_CONTROL_COUNT,
            2 * OSC_CONTROL_COUNT,
        };

        enum OscControl
        {
            CONTROL_OSC_TYPE = 0,
            CONTROL_OSC_VOL = 1,
            CONTROL_OSC_PAN = 2,
            CONTROL_OSC_COARSE = 3,
            CONTROL_OSC_FINE = 4,

            CONTROL_AMP_ATTACK = OSC_CONTROL_COUNT * OSC_COUNT,
            CONTROL_AMP_SUSTAIN,
            CONTROL_AMP_DECAY,
            CONTROL_AMP_RELEASE,

            CONTROL_FILTER_ATTACK,
            CONTROL_FILTER_SUSTAIN,
            CONTROL_FILTER_DECAY,
            CONTROL_FILTER_RELEASE,

            CONTROL_FILTER_TYPE,
            CONTROL_FILTER_FREQ,
            CONTROL_FILTER_RESO,
            CONTROL_FILTER_ENV,

            CONTROL_VIBRATO_DELAY,
            CONTROL_VIBRATO_SPEED,
            CONTROL_VIBRATO_AMOUNT
        };

        enum FilterType
        {
            FILTER_LOW_PASS,
            FILTER_HIGH_PASS
        };

        enum WaveformType
        {
            WAVE_SQUARE,
            WAVE_SAWTOOTH,
            WAVE_PULSE, // 1/4 pulse wave
            WAVE_TRIANGLE,
            WAVE_SINE,
        };

        WaveformModule(modules::ModuleCreator &create);

        void process(modules::ModuleProcessor &proc) override;
        void ui() override;
        bool has_presets() override { return true; };
    }; // class OscModule
}