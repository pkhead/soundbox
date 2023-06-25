#pragma once
#include "../plugins.h"

extern "C" {
#include <lilv/lilv.h>
}

namespace plugins
{
    class Lv2Plugin : public Plugin
    {
    private:
        audiomod::DestinationModule& dest;
    
    public:
        Lv2Plugin(audiomod::DestinationModule& dest, const PluginData& data);
        ~Lv2Plugin();

        virtual PluginType plugin_type() { return PluginType::Lv2; };

        static std::vector<PluginData> get_data(const char* path);

        void start() override;
        void stop() override;
        void process(float** inputs, float* output, size_t num_inputs, size_t buffer_size) override;
        void save_state(std::ostream& ostream) const override;
        bool load_state(std::istream& istream, size_t size) override;

        static const char* get_standard_paths();
        static void scan_plugins(const std::vector<std::string>& paths, std::vector<PluginData>& data_out);
        static void lilv_init();
        static void lilv_fini();
    }; // class LadspaPlugin
} // namespace plugins