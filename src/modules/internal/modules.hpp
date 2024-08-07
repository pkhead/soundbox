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
        
        struct Voice
        {
            bool active;
            int key;
            float freq;
            float volume;

            float phase;
        };

        Voice voices[MAX_VOICES];
    public:
        OscModule(modules::ModuleCreator &create);

        void process(modules::ModuleProcessor &proc) override;
        void ui() override;
    }; // class OscModule
}