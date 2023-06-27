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
#include "plugin_hosts/lv2.h"
#include "sys.h"

using namespace plugins;

Plugin::Plugin(const PluginData& data) : data(data)
{}

///////////////////
// Plugin Module //
///////////////////

PluginModule::PluginModule(Plugin* plugin)
    : ModuleBase(plugin->control_value_count() > 0), _plugin(plugin)
{
    name = _plugin->data.name;
    id = _plugin->data.id.c_str();
    _plugin->start();
}

PluginModule::~PluginModule()
{
    _plugin->stop();
    delete _plugin;
}

void PluginModule::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) {
    _plugin->song = song;
    _plugin->process(inputs, output, num_inputs, buffer_size);
}

void PluginModule::event(const audiomod::MidiEvent* event)
{
    return _plugin->event(event);
}

size_t PluginModule::receive_events(void** handle, audiomod::MidiEvent* buffer, size_t capacity)
{
    return _plugin->receive_events(handle, buffer, capacity);
}

void PluginModule::flush_events() {
    return _plugin->flush_events();
}

void PluginModule::save_state(std::ostream& stream) const
{
     _plugin->save_state(stream);
}

bool PluginModule::load_state(std::istream& stream, size_t size)
{
    return _plugin->load_state(stream, size);
}

void PluginModule::_interface_proc()
{
    int inputs_per_col = _plugin->control_value_count() / 2;
    if (inputs_per_col < 8) inputs_per_col = 8;
    
    ImGui::PushItemWidth(ImGui::GetTextLineHeight() * 14.0f);

    for (int i = 0; i < _plugin->control_value_count(); i += inputs_per_col)
    {
        if (i > 0) ImGui::SameLine();

        // write labels
        ImGui::BeginGroup();

        for (int j = i; j < _plugin->control_value_count() && j < i + inputs_per_col; j++)
        {
            auto control_value = _plugin->get_control_value(j);
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s", control_value.name);
        }

        ImGui::EndGroup();

        // make inputs
        ImGui::SameLine();
        ImGui::BeginGroup();

        for (int j = i; j < _plugin->control_value_count() && j < i + inputs_per_col; j++)
        {
            auto control_value = _plugin->get_control_value(j);

            ImGui::PushID(control_value.value);

            float value = *control_value.value;
            float min = control_value.min;
            float max = control_value.max;

            ImGuiSliderFlags log_flag = (control_value.is_logarithmic ? ImGuiSliderFlags_Logarithmic : 0);
            
            if (control_value.is_sample_rate) {
                value /= _dest->sample_rate;
                min /= _dest->sample_rate;
                max /= _dest->sample_rate;
            }

            if (control_value.is_toggle)
            {
                bool toggle = value >= 0.0f;
                ImGui::Checkbox("##toggle", &toggle);
                value = toggle ? 1.0f : -1.0f;
            }
            else if (control_value.is_integer)
            {
                int integer = (int)roundf(value);
                ImGui::SliderInt(
                    "##slider",
                    &integer,
                    (int)roundf(min),
                    (int)roundf(max),
                    control_value.format,
                    log_flag
                );
                value = integer;

                if (control_value.has_default && ImGui::IsItemClicked(ImGuiMouseButton_Middle)) {
                    value = control_value.default_value;
                }
            }
            else
            {
                ImGui::SliderFloat(
                    "##slider",
                    &value,
                    min,
                    max,
                    control_value.format,
                    ImGuiSliderFlags_NoRoundToFormat |
                    log_flag
                );

                if (control_value.has_default && ImGui::IsItemClicked(ImGuiMouseButton_Middle)) {
                    value = control_value.default_value;
                }
            }

            if (control_value.is_sample_rate) value *= _dest->sample_rate;
            *control_value.value = value;

            ImGui::PopID();
        }

        ImGui::EndGroup();
    }

    ImGui::PopItemWidth();

    for (int i = 0; i < _plugin->output_value_count(); i++)
    {
        auto output_value = _plugin->get_output_value(i);
        ImGui::Text("%s: %.3f", output_value.name, *output_value.value);
    }
}

////////////////////
// Plugin Manager //
////////////////////

static std::vector<std::string> parse_path_list(const std::string list_string)
{
    std::vector<std::string> list;
    std::stringstream stream(list_string);
    std::string item;

    const char delim =
#ifdef _WIN32
        ';';
#else
        ':';
#endif

    while (std::getline(stream, item, delim)) {
        list.push_back(item);
    }

    return list;
}

PluginManager::PluginManager()
{
    // get standard paths for plugin standards
    _std_ladspa = parse_path_list( LadspaPlugin::get_standard_paths() );
    _std_lv2 = parse_path_list( Lv2Plugin::get_standard_paths() );

    ladspa_paths = _std_ladspa;
    lv2_paths = _std_lv2;

    Lv2Plugin::lilv_init();
}

PluginManager::~PluginManager()
{
    Lv2Plugin::lilv_fini();
}

void PluginManager::add_path(PluginType type, const std::string& path)
{
    std::vector<std::string>* vec;

    switch (type)
    {
        case PluginType::Ladspa:
            vec = &ladspa_paths;
            break;

        case PluginType::Lv2:
            vec = &lv2_paths;
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

const std::vector<std::string>& PluginManager::get_standard_plugin_paths(PluginType type) const
{
    switch (type)
    {
        case PluginType::Ladspa:
            return _std_ladspa;

        case PluginType::Lv2:
            return _std_lv2;

        default: {
            // return empty list
            return _std_dummy;
        }
    }
}

void PluginManager::scan_plugins()
{
    plugin_data.clear();
    LadspaPlugin::scan_plugins(ladspa_paths, plugin_data);
    Lv2Plugin::scan_plugins(lv2_paths, plugin_data);
}