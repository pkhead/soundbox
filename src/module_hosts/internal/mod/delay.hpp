#pragma once
#include "audio_engine/audio_engine.hpp"
#include <module_hosts/modules.hpp>
#include <dsp.hpp>

namespace hosts::internal {
    /**
    * ID: sbox::delay
    * Simple delay/echo effect
    **/
    class DelayModule : public modx::ModuleBase {
    private:
        dsp::DelayLine<float> delay_line[2];
    public:
        enum ControlIndex {
            CONTROL_FEEDBACK,
            CONTROL_MIX,
            CONTROL_DELAY_SECS_L,
            CONTROL_DELAY_DIV_L,
            CONTROL_DELAY_SECS_R,
            CONTROL_DELAY_DIV_R,
            CONTROL_USE_TEMPO,
            CONTROL_STEREO_LOCK,
        };

        DelayModule(modules::ModuleCreator &create);

        void process(modules::ModuleProcessor &proc) override;
        void ui() override;
        bool has_presets() override { return true; }
    }; // class DelayModule
} // namespace hosts::internal