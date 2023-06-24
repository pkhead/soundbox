#include <cstdlib>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <string>

#include "plugins.h"
using namespace plugins;

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
    plugin_list.clear();

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

                if (path.has_filename())
                    std::cout << "found LADSPA: " << path << "\n";
                
            }
        }
    }
}