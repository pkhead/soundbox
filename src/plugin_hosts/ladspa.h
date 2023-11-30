#pragma once
#include <plugins/ladspa.h>
#include <filesystem>
#include "../plugins.h"

namespace plugins
{
    class LadspaPlugin : public PluginModule
    {
    private:
        sys::dl_handle lib;
        const LADSPA_Descriptor* descriptor;
        LADSPA_Handle instance;

        float* input_combined;
        std::vector<float*> input_buffers;
        std::vector<float*> output_buffers;

        struct ControlInput
        {
            std::string name;
            int port_index;
            float value;

            bool is_toggle;
            bool is_logarithmic;
            bool is_sample_rate;
            bool is_integer;
            bool has_default;

            float min, max;
            float default_value;
        };

        struct ControlOutput
        {
            std::string name;
            int port_index;
            float value;
        };

        std::vector<ControlInput*> ctl_in;
        std::vector<ControlOutput*> ctl_out;
    
    public:
        LadspaPlugin(audiomod::ModuleContext& modctx, const PluginData& data);
        ~LadspaPlugin();

        virtual PluginType plugin_type() { return PluginType::Ladspa; };

        void process(
            float** inputs,
            float* output,
            size_t num_inputs,
            size_t buffer_size,
            int sample_rate,
            int channel_count
        ) override;
        void save_state(std::ostream& ostream) override;
        bool load_state(std::istream& istream, size_t size) override;

        static const char* get_standard_paths();
        static void scan_plugins(const std::vector<std::filesystem::path>& paths, std::vector<PluginData>& data_out);

        virtual int control_value_count() const override;
        virtual int output_value_count() const override;

        virtual void set_control_value(int index, float value) override;
        virtual ControlValue get_control_value(int index) override;
        
        virtual OutputValue get_output_value(int index) override;
    }; // class LadspaPlugin
} // namespace plugins