#pragma once
#include "../plugins.h"
#include <plugins/ladspa.h>

namespace plugins
{
    class LadspaPlugin : public Plugin
    {
    private:
        sys::dl_handle lib;
        const LADSPA_Descriptor* descriptor;
        LADSPA_Handle instance;
        audiomod::DestinationModule& dest;

        std::vector<float*> input_buffers;
        std::vector<float*> output_buffers;
        std::vector<float*> output_ports;
    
    public:
        LadspaPlugin(audiomod::DestinationModule& dest, const PluginData& data);
        ~LadspaPlugin();

        virtual PluginType plugin_type() { return PluginType::Ladspa; };

        void start() override;
        void stop() override;
        void process(float** inputs, float* output, size_t num_inputs, size_t buffer_size) override;
        void save_state(std::ostream& ostream) const override;
        bool load_state(std::istream& istream, size_t size) override;

        static const char* get_standard_paths();
        static void scan_plugins(const std::vector<std::string>& paths, std::vector<PluginData>& data_out);
    }; // class LadspaPlugin
} // namespace plugins