#pragma once
#include "../modules.hpp"

namespace hosts::internal
{
    ///////////////////
    // sbox::midi_in //
    ///////////////////
    class MidiInputModule : public modx::ModuleBase
    {
    public:
        RingBuffer<std::byte> midi_queue;

        MidiInputModule(modules::ModuleCreator &create);
        void process(modules::ModuleProcessor &proc) override;
    }; // class MidiInputModule

    /////////////////
    // sbox::fader //
    /////////////////
    class FaderModule : public modx::ModuleBase
    {
    public:
        enum FaderControl
        {
            FADER_CONTROL_GAIN,
            FADER_CONTROL_PAN,
            FADER_CONTROL_MUTE,
        };
        
        FaderModule(modules::ModuleCreator &create);
        void process(modules::ModuleProcessor &proc) override;
    };

    ///////////////
    // sbox::osc //
    ///////////////
    class OscModule : public modx::ModuleBase
    {
    private:
        static constexpr unsigned int MAX_VOICES = 8;
        static constexpr unsigned int OSC_COUNT = 3;
        
        struct Voice
        {
            bool active;
            int key;
            float freq;
            float volume;

            float phase;
        };

        Voice voices[MAX_VOICES];

        int ui_selected_osc;
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
            CONTROL_FILTER_ENV
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
            WAVE_PULSE,
            WAVE_TRIANGLE,
            WAVE_SINE,
        };

        OscModule(modules::ModuleCreator &create);

        void process(modules::ModuleProcessor &proc) override;
        void ui() override;
    }; // class OscModule
}