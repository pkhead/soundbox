#pragma once
#include <cstdint>
#include <pluginapis/ladspa.h>
#include <sys.hpp>
#include "../modules.hpp"

namespace hosts::lv1
{
    class Lv1Module : public modx::ModuleBase
    {
    private:
        enum ControlInputFlags : uint8_t
        {
            INPUT_TOGGLE = 1,
            INPUT_LOGARITHMIC = 2,
            INPUT_SAMPLE_RATE = 4,
            INPUT_INTEGER = 8,
            INPUT_HAS_DEFAULT = 16
        };

        struct ControlInput
        {
            std::string name;
            int port_index;
            float value;
            uint8_t flags;

            float min, max;
            float default_value;
        };

        struct ControlOutput
        {
            std::string name;
            int port_index;
            float value;
        };

        std::vector<std::unique_ptr<ControlInput>> ctl_in;
        std::vector<std::unique_ptr<ControlOutput>> ctl_out;
        std::vector<float*> input_buffers;
        std::vector<float*> output_buffers;

        const LADSPA_Descriptor *descriptor;
        LADSPA_Handle instance;

    public:
        Lv1Module(modules::ModuleCreator &create, const LADSPA_Descriptor *plugin_desc, LADSPA_Handle instance);
        ~Lv1Module();

        void process(modules::ModuleProcessor &proc) override;
        void ui() override;
    }; // class Lv1Module
}