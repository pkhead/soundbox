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
        FaderModule(modules::ModuleCreator &create);
        void process(modules::ModuleProcessor &proc) override;
    };

    ///////////////
    // sbox::osc //
    ///////////////
    class OscModule : public modx::ModuleBase
    {
    private:
        float phase;
        float freq;

    public:
        OscModule(modules::ModuleCreator &create);

        void process(modules::ModuleProcessor &proc) override;
        void ui() override;
    }; // class OscModule
}