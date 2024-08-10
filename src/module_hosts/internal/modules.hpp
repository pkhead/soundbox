#pragma once
#include <atomic>
#include <dsp.hpp>
#include "../modules.hpp"

#include "mod/channel_control.hpp"
#include "mod/analyzer.hpp"
#include "mod/waveform.hpp"

namespace hosts::internal
{
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