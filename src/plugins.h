#pragma once
#include <string>
#include <vector>

namespace plugins
{
    enum class PluginType : uint8_t
    {
        Ladspa,
        Lv2,  // TODO
        Vst,  // TODO
        Clap ,// TODO
    };

    struct PluginListing
    {
        const char* name;
        const char* file_path;
        PluginType plugin_type;
        bool is_instrument;
    };

    /**
    * Class to handle discovery of plugins and plugin information
    **/
    class PluginManager
    {
    private:
        std::vector<PluginListing> plugin_list;

    public:
        std::vector<std::string> ladspa_paths;
        
        PluginManager();
        inline const std::vector<PluginListing>& get_plugins() { return plugin_list; };
        void scan_plugins();

        static std::vector<std::string> get_default_plugin_paths(PluginType type);
    };
} // namespace plugin