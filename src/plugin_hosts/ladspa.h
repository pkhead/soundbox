#pragma once
#include "../plugins.h"
#include <plugins/ladspa.h>

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
        LadspaPlugin(audiomod::DestinationModule& dest, const PluginData& data);
        ~LadspaPlugin();

        virtual PluginType plugin_type() { return PluginType::Ladspa; };

        void start() override;
        void stop() override;
        void process(
            float** inputs,
            float* output,
            size_t num_inputs,
            size_t buffer_size,
            int sample_rate,
            int channel_count
        ) override;
        void save_state(std::ostream& ostream) const override;
        bool load_state(std::istream& istream, size_t size) override;

        // ladspa plugins cannot send or receive midi events, so these are no ops
        virtual void event(const audiomod::MidiEvent* event) override {};
        virtual size_t receive_events(void** handle, audiomod::MidiEvent* buffer, size_t capacity) override {
            return 0; 
        };
        virtual void flush_events() override {};

        static const char* get_standard_paths();
        static void scan_plugins(const std::vector<std::string>& paths, std::vector<PluginData>& data_out);

        virtual int control_value_count() const override;
        virtual int output_value_count() const override;
        virtual ControlValue get_control_value(int index) override;
        virtual OutputValue get_output_value(int index) override;
    }; // class LadspaPlugin
} // namespace plugins