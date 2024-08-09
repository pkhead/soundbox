#pragma once
#include "../modules.hpp"
#include "dsp.h"
#include "modules/internal/midi.hpp"
#include "mod/waveform.hpp"

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

    ////////////////
    // sbox::gain //
    ////////////////
    class GainModule : public modx::ModuleBase
    {
    private:

    public:
        GainModule(modules::ModuleCreator &create);

        void process(modules::ModuleProcessor &proc) override;
        void ui() override;
    }; // class GainModule
}