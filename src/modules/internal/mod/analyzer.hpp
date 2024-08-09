#pragma once
#include <modules/modules.hpp>
#include "../dsp.h"

namespace hosts::internal
{
    ////////////////////
    // sbox::analyzer //
    ////////////////////
    class AnalyzerModule : public modx::ModuleBase
    {
    private:
        const int frames_per_window = 1024;
        const int window_margin = 512; // in frames

        size_t queue_capacity;
        RingBuffer<float> audio_queue_left;
        RingBuffer<float> audio_queue_right;

        struct
        {
            float *buf_left;
            float *buf_right;
            unsigned int index;
        } ui_state;

        struct
        {
            float *buf_left;
            float *buf_right;
        } audio_state;

        // oscilloscope windows
        //float* window_left[2] = { nullptr, nullptr };
        //float* window_right[2] = { nullptr, nullptr };
    public:
        AnalyzerModule(modules::ModuleCreator &create);
        ~AnalyzerModule();

        void process(modules::ModuleProcessor &proc) override;
        void ui() override;
        bool has_presets() override { return false; };
    }; // class AnalyzerModule
}