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

        std::string name;
        std::string author;
        std::string copyright;

        bool is_instrument;
    };

    // base class for plugins
    class Plugin
    {
    protected:
        bool _is_instrument;
    public:
        std::string name;
        std::string file_path;

        Plugin();
        virtual ~Plugin() {};

        virtual PluginType plugin_type() = 0;
        inline bool is_instrument() const { return _is_instrument; }
    };

    class LadspaPlugin : public Plugin
    {
    public:
        LadspaPlugin(sys::dl_handle lib, int index);
        ~LadspaPlugin();

        virtual PluginType plugin_type() { return PluginType::Ladspa; };

        static std::vector<PluginData> get_data(const char* path);
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