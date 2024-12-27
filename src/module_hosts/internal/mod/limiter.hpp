#pragma once
#include "audio_engine/audio_engine.hpp"
#include "audio_engine/ring_buffer.hpp"
#include <module_hosts/modules.hpp>
#include <dsp.hpp>

namespace hosts::internal {
    /**
    * ID: sbox::limiter
    **/
    class LimiterModule : public modx::ModuleBase {
    private:
        float cur_limit[2];
        dsp::VUMeter vu_in;
        dsp::VUMeter vu_out;

        struct MeterData {
            float in_level;
            float out_level;
            float in_peak;
            float out_peak;
        };

        RingBuffer<MeterData> msg_queue;

        MeterData cur_analysis;

        static void idle_proc(modules::AudioEngine &engine, modules::ModuleID id, void *userdata);
    public:
        enum ControlIndex {
            CONTROL_INPUT_GAIN,
            CONTROL_OUTPUT_GAIN,
            CONTROL_THRESHOLD,
            CONTROL_ATTACK,
            CONTROL_DECAY,
        };

        LimiterModule(modules::ModuleCreator &create);

        void process(modules::ModuleProcessor &proc) override;
        void ui() override;
        bool has_presets() override { return false; }
    };
} // namespace hosts::internal