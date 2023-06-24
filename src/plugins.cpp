#include <cstdlib>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <cmath>
#include <imgui.h>
#include <plugins/ladspa.h>

#include "audio.h"
#include "plugins.h"
#include "plugin_hosts/ladspa.h"
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
// Plugin Module //
///////////////////

PluginModule::PluginModule(Plugin* plugin)
    : ModuleBase(plugin->control_values.size() > 0), _plugin(plugin)
{
    name = _plugin->data.name;
    _plugin->start();
}

PluginModule::~PluginModule()
{
    _plugin->stop();
    delete _plugin;
}

void PluginModule::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) {
    // TODO: call plugin run process
    _plugin->process(inputs, output, num_inputs, buffer_size);
}

void PluginModule::_interface_proc()
{
    for (auto& control_value : _plugin->control_values)
    {
        ImGui::PushID(&control_value);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s", control_value->name.c_str());
        ImGui::SameLine();

        float value = control_value->value;
        float min = control_value->min;
        float max = control_value->max;
        
        if (control_value->is_sample_rate) {
            value /= _dest->sample_rate;
            min /= _dest->sample_rate;
            max /= _dest->sample_rate;
        }

        // TODO is_logarithmic
        if (control_value->is_toggle)
        {
            bool toggle = value >= 0.0f;
            ImGui::Checkbox("##toggle", &toggle);
            value = toggle ? 1.0f : -1.0f;
        }
        else if (control_value->is_integer)
        {
            int integer = (int)roundf(value);
            ImGui::SliderInt("##slider", &integer, (int)roundf(min), (int)roundf(max));
            value = integer;

            if (control_value->has_default && ImGui::IsItemClicked(ImGuiMouseButton_Middle)) {
                value = control_value->default_value;
            }
        }
        else
        {
            ImGui::SliderFloat("##slider", &value, min, max, "%.3f");

            if (control_value->has_default && ImGui::IsItemClicked(ImGuiMouseButton_Middle)) {
                value = control_value->default_value;
            }
        }


        if (control_value->is_sample_rate) value *= _dest->sample_rate;
        control_value->value = value;

        ImGui::PopID();
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

void PluginManager::add_path(PluginType type, const std::string& path)
{
    std::vector<std::string>* vec;

    switch (type)
    {
        case PluginType::Ladspa:
            vec = &ladspa_paths;
            break;

        default:
            throw std::runtime_error("unsupported PluginType");
    }

    // don't add path if path is already in list
    for (std::string& v : *vec)
    {
        if (v == path) return;
    }

    vec->push_back(path);
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