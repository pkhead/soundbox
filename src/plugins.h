#pragma once
#include <string>
#include <vector>
#include "sys.h"
#include "audio.h"
#include "sys.h"
#include "winmgr.h"
#include "worker.h"

namespace plugins
{
    class module_create_error : public std::runtime_error
    {
    public:
        module_create_error(const std::string& what = "") : std::runtime_error(what) {};
    };

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
    class PluginModule : public audiomod::ModuleBase {
    protected:
        virtual void _interface_proc() override;
        
        struct ControlValue
        {
            const char* name; // name of the control
            const char* format; // display format string
            float value;

            bool is_toggle;
            bool is_logarithmic;
            bool is_sample_rate;
            bool is_integer;
            bool has_default;

            float min, max;
            float default_value;
        };

        struct OutputValue
        {
            const char* name;
            const char* value;
        };

        const PluginData& data;

        virtual int control_value_count() const = 0;
        virtual int output_value_count() const = 0;

        virtual ControlValue get_control_value(int index) = 0;
        virtual void set_control_value(int index, float value) = 0;

        virtual OutputValue get_output_value(int index) = 0;

        // tell implementation that a control value has changed
        virtual void control_value_change(int index) {};

    public:
        PluginModule(audiomod::DestinationModule& audio_dest, const PluginData& data);
        virtual ~PluginModule() {};
    };

    /**
    * Class to handle discovery of plugins and plugin information
    **/
    class PluginManager
    {
    private:
        std::vector<PluginData> plugin_data;

        std::vector<std::string> _std_ladspa;
        std::vector<std::string> _std_lv2;
        std::vector<std::string> _std_dummy; // empty vector

        WindowManager& window_manager;
    public:
        std::vector<std::string> ladspa_paths;
        std::vector<std::string> lv2_paths;
        
        PluginManager(WindowManager& window_manager);

        PluginModule* instantiate_plugin(
            const PluginData& plugin_data,
            audiomod::DestinationModule& audio_dest,
            WorkScheduler& work_scheduler
        );

        void add_path(PluginType type, const std::string& path);
        inline const std::vector<PluginData>& get_plugin_data() { return plugin_data; };
        void scan_plugins();

        const std::vector<std::string>& get_standard_plugin_paths(PluginType type) const;
    };
} // namespace plugin