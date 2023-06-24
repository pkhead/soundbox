#include <cstdlib>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <plugins/ladspa.h>

#include "audio.h"
#include "plugins.h"
#include "sys.h"
using namespace plugins;

Plugin::Plugin(const PluginData& data) : data(data)
{}

Plugin::~Plugin()
{
    // free control values
    for (ControlValue* control_value : control_values)
    {
        delete control_value;
    }
}

///////////////////
// LADSPA plugin //
///////////////////

std::vector<PluginData> LadspaPlugin::get_data(const char *path)
{
    std::vector<PluginData> output;
    sys::dl_handle handle = sys::dl_open(path);

    if (handle == nullptr)
    {
        std::cerr << "dl error: " << sys::dl_error() << "\n";
        sys::dl_close(handle);
        return output; // return empty vector
    }

    // retrieve descriptor function
    LADSPA_Descriptor_Function ladspa_descriptor =
        (LADSPA_Descriptor_Function) sys::dl_sym(handle, "ladspa_descriptor");

    if (ladspa_descriptor == nullptr)
    {
        std::cerr << "dl error: " << sys::dl_error() << "\n";
        sys::dl_close(handle);
        return output; // return empty vector
    }

    // get descriptors from an increasing index
    // if plugin_data == nullptr, there are no more plugins
    const LADSPA_Descriptor* plugin_desc;
    for (int i = 0; (plugin_desc = ladspa_descriptor(i)) != nullptr; i++)
    {
        PluginData data;
        data.type = PluginType::Ladspa;
        data.file_path = path;
        data.index = i;

        // id from file path and plugin label
        data.id = std::string("plugins.ladspa:") + plugin_desc->Label + "@" + std::filesystem::path(path).stem().string();
        data.name = plugin_desc->Name;
        data.author = plugin_desc->Maker;
        data.copyright = plugin_desc->Copyright;

        // if plugin has no input audio ports, it is an instrument
        data.is_instrument = true;
        for (int port_i = 0; port_i < plugin_desc->PortCount; port_i++)
        {
            LADSPA_PortDescriptor port_descriptor = plugin_desc->PortDescriptors[port_i];

            if (LADSPA_IS_PORT_INPUT(port_descriptor) && LADSPA_IS_PORT_AUDIO(port_descriptor)) {
                data.is_instrument = false;
                break;
            }
        }
        
        output.push_back(data);
    }

    sys::dl_close(handle);
    return output;
}

// constructor
LadspaPlugin::LadspaPlugin(audiomod::DestinationModule& dest, const PluginData& plugin_data)
    : Plugin(plugin_data)
{
    lib = sys::dl_open(plugin_data.file_path.c_str());
    if (lib == nullptr)
    {
        throw std::runtime_error(sys::dl_error());
    }

    LADSPA_Descriptor_Function ladspa_descriptor =
        (LADSPA_Descriptor_Function) sys::dl_sym(lib, "ladspa_descriptor");

    if (ladspa_descriptor == nullptr)
    {
        auto err = std::runtime_error(sys::dl_error());
        sys::dl_close(lib);
        throw err;
    }

    descriptor = ladspa_descriptor(plugin_data.index);
    if (descriptor == nullptr)
    {
        sys::dl_close(lib);
        throw std::runtime_error(std::string("no plugin at index ") + std::to_string(plugin_data.index) + " of " + plugin_data.file_path);
    }

    instance = descriptor->instantiate(descriptor, dest.sample_rate);
    if (instance == nullptr)
    {
        sys::dl_close(lib);
        throw std::runtime_error(std::string("could not instantiate ") + plugin_data.name);
    }
}

LadspaPlugin::~LadspaPlugin()
{
    descriptor->cleanup(instance);
    sys::dl_close(lib);
}

void LadspaPlugin::start()
{}

void LadspaPlugin::stop()
{}

void LadspaPlugin::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size)
{}












///////////////////
// Plugin Module //
///////////////////

PluginModule::PluginModule(Plugin* plugin)
    : ModuleBase(true), _plugin(plugin)
{
    name = _plugin->data.name;
}

PluginModule::~PluginModule()
{
    delete _plugin;
}

void PluginModule::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) {
    // TODO: call plugin run process
    for (size_t i = 0; i < buffer_size; i += channel_count) {
        output[i] = 0.0f;
        output[i+1] = 0.0f;

        for (size_t k = 0; k < num_inputs; k++)
        {
            output[i] += inputs[k][i];
            output[i+1] += inputs[k][i+1];
        }
    }
}

////////////////////
// Plugin Manager //
////////////////////
PluginManager::PluginManager()
{
    ladspa_paths = get_default_plugin_paths(PluginType::Ladspa);
}

static std::vector<std::string> parse_path_list(const std::string list_string)
{
    std::vector<std::string> list;
    std::stringstream stream(list_string);
    std::string item;

    while (std::getline(stream, item, ':')) {
        list.push_back(item);
    }

    return list;
}

std::vector<std::string> PluginManager::get_default_plugin_paths(PluginType type)
{
    switch (type)
    {
        case PluginType::Ladspa: {
            const char* list_str = std::getenv("LADSPA_PATH");
            if (list_str == nullptr)
                list_str = "/usr/lib/ladspa:/usr/local/lib/ladspa";

            return parse_path_list(list_str);
        }

        default: {
            // return empty list
            return std::vector<std::string>();
        }
    }
}

void PluginManager::scan_plugins()
{
    plugin_data.clear();

    // scan directories for ladspa plugins
    for (const std::string& directory : ladspa_paths)
    {
        if (std::filesystem::exists(directory) && std::filesystem::is_directory(directory))
        {
            for (const auto& entry : std::filesystem::directory_iterator(directory))
            {
                // don't read directories
                if (entry.is_directory()) continue;

                auto& path = entry.path();
                std::cout << "found LADSPA: " << path << ":\n";
                
                // get plugin information for all plugins in library
                std::vector<PluginData> plugins = LadspaPlugin::get_data(entry.path().c_str());

                for (PluginData& plugin : plugins)
                {
                    std::cout << "\t" << plugin.name << " by " << plugin.author << "\n";
                    plugin_data.push_back(plugin);
                }
            }
        }
    }
}