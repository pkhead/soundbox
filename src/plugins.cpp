#include <cstdlib>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <cmath>
#include <imgui.h>

#include "audio.h"
#include "plugins.h"
#include "plugin_hosts/ladspa.h"
#include "sys.h"

#ifdef ENABLE_LV2
#include "plugin_hosts/lv2-host/lv2interface.h"
#endif

using namespace plugins;

///////////////////
// Plugin Module //
///////////////////

PluginModule::PluginModule(audiomod::ModuleContext& modctx, const PluginData& plugin_data)
    : ModuleBase(false), modctx(modctx), data(plugin_data)
{
    name = plugin_data.name;
    id = plugin_data.id.c_str();
}

void PluginModule::_interface_proc()
{
    int inputs_per_col = control_value_count() / 2;
    if (inputs_per_col < 8) inputs_per_col = 8;
    
    ImGui::PushItemWidth(ImGui::GetTextLineHeight() * 14.0f);

    for (int i = 0; i < control_value_count(); i += inputs_per_col)
    {
        if (i > 0) ImGui::SameLine();

        // write labels
        ImGui::BeginGroup();

        for (int j = i; j < control_value_count() && j < i + inputs_per_col; j++)
        {
            auto control_value = get_control_value(j);
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s", control_value.name);
        }

        ImGui::EndGroup();

        // make inputs
        ImGui::SameLine();
        ImGui::BeginGroup();

        for (int j = i; j < control_value_count() && j < i + inputs_per_col; j++)
        {
            auto control_value = get_control_value(j);

            ImGui::PushID(j);

            float value = control_value.value;
            float min = control_value.min;
            float max = control_value.max;

            ImGuiSliderFlags log_flag = (control_value.is_logarithmic ? ImGuiSliderFlags_Logarithmic : 0);
            
            if (control_value.is_sample_rate) {
                value /= modctx.sample_rate;
                min /= modctx.sample_rate;
                max /= modctx.sample_rate;
            }

            if (control_value.is_toggle)
            {
                bool toggle = value >= 0.0f;
                if (ImGui::Checkbox("##toggle", &toggle)) {
                    control_value_change(j);
                }

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
                    control_value_change(j);
                }

                if (ImGui::IsItemActive())
                    control_value_change(j);
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
                    control_value_change(j);
                }
            
                if (ImGui::IsItemActive())
                    control_value_change(j);
            }


            if (control_value.is_sample_rate) value *= modctx.sample_rate;

            if (control_value.value != value)
                set_control_value(j, value);

            ImGui::PopID();
        }

        ImGui::EndGroup();
    }

    ImGui::PopItemWidth();

    for (int i = 0; i < output_value_count(); i++)
    {
        auto output_value = get_output_value(i);
        ImGui::Text("%s: %s", output_value.name, output_value.value);
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

PluginManager::PluginManager(WindowManager& win_manager) : window_manager(win_manager)
{
    // get standard paths for plugin standards
    _std_ladspa = parse_path_list( LadspaPlugin::get_standard_paths() );

#ifdef ENABLE_LV2
    _std_lv2 = parse_path_list( Lv2Plugin::get_standard_paths() );
#endif
}

void PluginManager::add_path(PluginType type, const std::string& path)
{
    std::vector<std::string>* vec;

    switch (type)
    {
        case PluginType::Ladspa:
            vec = &user_ladspa_paths;
            break;

        case PluginType::Lv2:
            vec = &user_lv2_paths;
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

void PluginManager::remove_path(PluginType type, const std::string& path)
{
    std::vector<std::string>* vec;

    switch (type)
    {
        case PluginType::Ladspa:
            vec = &user_ladspa_paths;
            break;

        case PluginType::Lv2:
            vec = &user_lv2_paths;
            break;

        default:
            throw std::runtime_error("unsupported PluginType");
    }

    auto it = std::find(vec->begin(), vec->end(), path);
    if (it != vec->end())
        vec->erase(it);
}

const std::vector<std::string>& PluginManager::get_user_paths(PluginType type) const
{
    switch (type)
    {
        case PluginType::Ladspa:
            return user_ladspa_paths;

        case PluginType::Lv2:
            return user_lv2_paths;

        default:
            throw std::runtime_error("unsupported plugin type");
    }
}

const std::vector<std::string>& PluginManager::get_system_paths(PluginType type) const
{
    switch (type)
    {
        case PluginType::Ladspa:
            return _std_ladspa;

        case PluginType::Lv2:
            return _std_lv2;

        default:
            throw std::runtime_error("unsupported plugin type");
    }
}

std::vector<std::string> PluginManager::get_paths(PluginType type) const
{   
    const std::vector<std::string> *std, *app, *user;

    switch (type)
    {
        case PluginType::Ladspa:
            std = &_std_ladspa;
            app = &ladspa_paths;
            user = &user_ladspa_paths;
            break;

        case PluginType::Lv2:
            std = &_std_lv2;
            app = &lv2_paths;
            user = &user_lv2_paths;

        default:
            return std::vector<std::string>();
    }

    assert(std && app && user);

    std::vector<std::string> res = *std;
    res.insert(res.end(), app->begin(), app->end());
    res.insert(res.end(), user->begin(), user->end());
    return res;
}

bool PluginManager::is_user_path(PluginType type, const std::string& path) const
{
    auto& user_paths = get_user_paths(type);
    return std::find(user_paths.begin(), user_paths.end(), path) != user_paths.end();
}

void PluginManager::scan_plugins()
{
    plugin_data.clear();

    LadspaPlugin::scan_plugins(get_paths(PluginType::Ladspa), plugin_data);
#ifdef ENABLE_LV2
    Lv2Plugin::scan_plugins(get_paths(PluginType::Lv2), plugin_data);
#endif
}

audiomod::ModuleNodeRc PluginManager::instantiate_plugin(
    const PluginData& plugin_data,
    audiomod::ModuleContext& modctx,
    WorkScheduler& work_scheduler
) {
    audiomod::ModuleNodeRc plugin;

    switch (plugin_data.type)
    {
        case plugins::PluginType::Ladspa:
            plugin = modctx.create<plugins::LadspaPlugin>(modctx, plugin_data);
            break;

#ifdef ENABLE_LV2
        case plugins::PluginType::Lv2:
        try {
            plugin = new plugins::Lv2Plugin(modctx, plugin_data, work_scheduler, window_manager);
        } catch (plugins::lv2_error& err) {
            throw module_create_error(err.what());
        }
            break;
#endif

        default:
            throw std::runtime_error("invalid plugin type");
    }

    return plugin;
}