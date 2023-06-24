#pragma once
#include <string>
#include <vector>
#include "sys.h"
#include "audio.h"
#include "sys.h"

namespace plugins
{
    enum class PluginType : uint8_t
    {
        Ladspa,
        Lv2,  // TODO
        Vst,  // TODO
        Clap ,// TODO
    };

    struct PluginData
    {
        std::string file_path;
        PluginType type;
        int index;

        std::string id; // internal soundbox id 
        std::string name;
        std::string author;
        std::string copyright;

        bool is_instrument;
    };

    // base class for plugins
    class Plugin {
    public:
        struct ControlValue
        {
            std::string name;
            float value;

            bool is_toggle;
            bool is_logarithmic;
            bool is_sample_rate;
            bool is_integer;
            bool has_default;

            float min, max;
            float default_value;
        };

        std::vector<ControlValue*> control_values;

        const PluginData data;

        Plugin(const PluginData& data);
        virtual ~Plugin();

        virtual void start() = 0;
        virtual void stop() = 0;
        virtual void process(float** inputs, float* output, size_t num_inputs, size_t buffer_size) = 0;
    };

    class PluginModule : public audiomod::ModuleBase
    {
    private:
        void _interface_proc() override;
        Plugin* _plugin;

    public:
        // assumes ownership of the plugin pointer
        // meaning it will be invalidated once this class is deleted
        PluginModule(Plugin* plugin);
        ~PluginModule();

        void process(
            float** inputs,
            float* output,
            size_t num_inputs,
            size_t buffer_size,
            int sample_rate,
            int channel_count
        ) override;
    };

    /**
    * Class to handle discovery of plugins and plugin information
    **/
    class PluginManager
    {
    private:
        std::vector<PluginData> plugin_data;

    public:
        std::vector<std::string> ladspa_paths;
        
        PluginManager();
        void add_path(PluginType type, const std::string& path);
        inline const std::vector<PluginData>& get_plugin_data() { return plugin_data; };
        void scan_plugins();

        static std::vector<std::string> get_default_plugin_paths(PluginType type);
    };
} // namespace plugin