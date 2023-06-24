#pragma once
#include <string>
#include <vector>
#include "plugins/ladspa.h"
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
        const PluginData data;

        Plugin(const PluginData& data);
        virtual ~Plugin() {};
    };

    class LadspaPlugin : public Plugin
    {
    private:
        sys::dl_handle lib;
        const LADSPA_Descriptor* descriptor;
        LADSPA_Handle instance;
    
    public:
        LadspaPlugin(const PluginData& data);
        ~LadspaPlugin();

        virtual PluginType plugin_type() { return PluginType::Ladspa; };

        static std::vector<PluginData> get_data(const char* path);
    };

    class PluginModule : public audiomod::ModuleBase
    {
    private:
        // void _interface_proc() override;
        Plugin* _plugin;

    public:
        // assumes ownership of the plugin pointer
        // meaning it will be invalidated once this class is deleted
        PluginModule(Song* song, Plugin* plugin);
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
        inline const std::vector<PluginData>& get_plugin_data() { return plugin_data; };
        void scan_plugins();

        static std::vector<std::string> get_default_plugin_paths(PluginType type);
    };
} // namespace plugin