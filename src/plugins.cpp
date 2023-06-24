#include <cstdlib>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <plugins/ladspa.h>

#include "plugins.h"
#include "sys.h"
using namespace plugins;

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

        data.name = plugin_desc->Name;
        data.author = plugin_desc->Maker;
        data.copyright = plugin_desc->Copyright;
        data.is_instrument = false;

        output.push_back(data);
    }

    sys::dl_close(handle);
    return output;
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