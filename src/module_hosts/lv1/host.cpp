#include <filesystem>
#include <cmath>
#include <sys.hpp>
#include <exception>
#include <pluginapis/ladspa.h>
#include "host.hpp"
#include "instance.hpp"

using namespace hosts::lv1;

const char* get_std_path_string()
{
    const char* list_str = std::getenv("LADSPA_PATH");
    if (list_str == nullptr)
#ifdef _WIN32
        list_str = ""; // windows has no standard paths for ladspa plugins
#else
        list_str = "/usr/lib/ladspa:/usr/local/lib/ladspa";
#endif

    return list_str;
}

bool Lv1ModuleHost::initialize()
{
    // collect search paths
    {
        std::filesystem::path::string_type paths_str = std::filesystem::u8path(get_std_path_string()).native();
        std::filesystem::path::string_type path_buf;

#ifdef _WIN32
        constexpr char path_sep = ';';
#else
        constexpr char path_sep = ':';
#endif

        for (std::filesystem::path::value_type &ch : paths_str)
        {
            if (ch == path_sep && !path_buf.empty())
            {
                search_paths.push_back(path_buf);
                path_buf.clear();
            }
            else
            {
                path_buf.push_back(ch);
            }
        }

        if (!path_buf.empty())
            search_paths.push_back(path_buf);
    }
    
    if (search_paths.empty())
        logger::log_warning("no search paths for LV1");

    return true;
}

const char* Lv1ModuleHost::host_id() const
{
    return "lv1";
}

void Lv1ModuleHost::get_plugin_info(std::filesystem::path dlpath, std::vector<modules::ModuleInfo> &out_mod_list)
{
    sys::dl_handle dl = sys::dl_handle::open(dlpath);

    if (!dl.is_open())
    {
        logger::log_error("error opening dll %s: %s", dlpath.u8string().c_str(), sys::dl_handle::error());
        return;
    }

    // retrieve descriptor function
    LADSPA_Descriptor_Function ladspa_descriptor =
        (LADSPA_Descriptor_Function) dl.sym("ladspa_descriptor");

    if (ladspa_descriptor == nullptr)
    {
        logger::log_error("could not obtain ladspa_descriptor from %s: %s", dlpath.u8string().c_str(), sys::dl_handle::error());
        return;
    }

    // get descriptors from an increasing index
    // if plugin_data == nullptr, there are no more plugins
    const LADSPA_Descriptor* plugin_desc;
    for (unsigned long i = 0; (plugin_desc = ladspa_descriptor(i)) != nullptr; i++)
    {
        // id from file path and plugin label
        std::string mod_class_name = std::string("lv1::") + plugin_desc->Label + "/" + std::to_string(plugin_desc->UniqueID);

        // check if plugin has input audio ports
        bool has_audio_input = false;

        for (int port_i = 0; port_i < plugin_desc->PortCount; port_i++)
        {
            LADSPA_PortDescriptor port_descriptor = plugin_desc->PortDescriptors[port_i];

            if (LADSPA_IS_PORT_INPUT(port_descriptor) && LADSPA_IS_PORT_AUDIO(port_descriptor)) {
                has_audio_input = true;
                break;
            }
        }

        modules::ModuleInfo mod_info(mod_class_name, plugin_desc->Name);
        mod_info.has_audio_input = has_audio_input;
        mod_info.has_midi_input = false;
        mod_info.author = plugin_desc->Maker;

        out_mod_list.push_back(mod_info);
        assert(_plugin_info.find(mod_class_name) == _plugin_info.end());
        _plugin_info[mod_class_name] = std::make_unique<PluginInfo>(dlpath, std::move(dl), i);

        logger::log_info("register lv1 plugin '%s' as %s", plugin_desc->Name, mod_class_name.c_str());
    }
}

const std::vector<modules::ModuleInfo> Lv1ModuleHost::scan_modules()
{
    _plugin_info.clear();
    std::vector<modules::ModuleInfo> list;

    for (const std::filesystem::path& directory : search_paths)
    {
        if (std::filesystem::exists(directory) && std::filesystem::is_directory(directory))
        {
            for (const auto& entry : std::filesystem::directory_iterator(directory))
            {
                // don't read directories
                if (entry.is_directory()) continue;
                auto& path = entry.path();
                
                // get plugin information for all plugins in library
                get_plugin_info(path, list);
            }
        }
    }
    
    return list;
}

bool Lv1ModuleHost::create_module(modules::ModuleCreator &create)
{
    const auto &plugin_info_it = _plugin_info.find(create.class_name);
    if (plugin_info_it == _plugin_info.end())
        return false;
    const std::unique_ptr<PluginInfo> &plugin_info = plugin_info_it->second;

    const sys::dl_handle &dl = plugin_info->dl;
    if (!dl.is_open())
    {
        logger::log_error("could not open dll %s", plugin_info->dl_path.u8string().c_str());
        return false;
    }

    LADSPA_Descriptor_Function ladspa_desc =
        (LADSPA_Descriptor_Function) dl.sym("ladspa_descriptor");
    
    if (ladspa_desc == nullptr)
    {
        logger::log_error("could not obtain ladspa_descriptor from %s: %s", plugin_info->dl_path.u8string().c_str(), sys::dl_handle::error());
        assert(false);
        return false;
    }

    const LADSPA_Descriptor *plugin_desc = ladspa_desc(plugin_info->index);
    if (plugin_desc == nullptr)
    {
        logger::log_error("could not obtain plugin descriptor for %s (index %lu)", create.class_name.c_str(), plugin_info->index); 
        assert(false);
        return false;
    }
    
    LADSPA_Handle instance = plugin_desc->instantiate(plugin_desc, create.engine.sample_rate());
    if (instance == nullptr)
    {
        logger::log_error("could not instantiate plugin %s", create.class_name.c_str());
        assert(false);
        return false;
    }

    modx::ModuleBase *mod = new Lv1Module(create, plugin_desc, instance);
    init_module(mod, create);
    return true;
}







/////////////////////////
// LV1 Module Instance //
/////////////////////////
Lv1Module::Lv1Module(modules::ModuleCreator &create, const LADSPA_Descriptor *p_desc, LADSPA_Handle p_instance) :
    modx::ModuleBase(create),
    descriptor(p_desc),
    instance(p_instance)
{
    unsigned long sample_rate = create.engine.sample_rate();

    // connect ports
    for (int port_i = 0; port_i < descriptor->PortCount; port_i++)
    {
        LADSPA_PortDescriptor port = descriptor->PortDescriptors[port_i];

        if (LADSPA_IS_PORT_CONTROL(port))
        {
            if (LADSPA_IS_PORT_INPUT(port))
            {
                std::unique_ptr<ControlInput> control = std::make_unique<ControlInput>();
                control->name = descriptor->PortNames[port_i];
                control->port_index = port_i;
                
                // use these values if unspecified
                control->min = -10.0f;
                control->max = 10.0f;
                control->default_value = 0.0f;
                control->flags = 0;
                
                // read hints
                const LADSPA_PortRangeHint& range_hint = descriptor->PortRangeHints[port_i];
                LADSPA_PortRangeHintDescriptor hint_descriptor = range_hint.HintDescriptor;

                bool is_toggle = LADSPA_IS_HINT_TOGGLED(hint_descriptor);
                bool is_log = LADSPA_IS_HINT_LOGARITHMIC(hint_descriptor);
                bool is_int = LADSPA_IS_HINT_INTEGER(hint_descriptor);
                bool is_sample_rate = LADSPA_IS_HINT_SAMPLE_RATE(hint_descriptor);

                if (is_toggle)     control->flags |= INPUT_TOGGLE;
                if (is_log)        control->flags |= INPUT_LOGARITHMIC;
                if (is_int)        control->flags |= INPUT_INTEGER;
                if (is_sample_rate)control->flags |= INPUT_SAMPLE_RATE;

                if (LADSPA_IS_HINT_BOUNDED_BELOW(hint_descriptor))
                    control->min =
                        range_hint.LowerBound *
                        (is_sample_rate ? sample_rate : 1) - // if control describes sample rate
                        (is_int ? -0.01f : 0.0f); // avoid floating point rounding errors

                if (LADSPA_IS_HINT_BOUNDED_ABOVE(hint_descriptor))
                    control->max =
                        range_hint.UpperBound *
                        (is_sample_rate ? sample_rate : 1) + // if control describes sample rate
                        (is_int ? 0.01f : 0.0f); // avoid floating point rounding errors

                // default value
                if (LADSPA_IS_HINT_HAS_DEFAULT(hint_descriptor))
                {
                    control->flags |= INPUT_HAS_DEFAULT;

                    if (LADSPA_IS_HINT_DEFAULT_0(hint_descriptor))
                        control->default_value = 0.0f;

                    else if (LADSPA_IS_HINT_DEFAULT_1(hint_descriptor))
                        control->default_value = 1.0f;

                    else if (LADSPA_IS_HINT_DEFAULT_100(hint_descriptor))
                        control->default_value = 100.0f;

                    else if (LADSPA_IS_HINT_DEFAULT_440(hint_descriptor))
                        control->default_value = 440.0f;

                    else if (LADSPA_IS_HINT_DEFAULT_MINIMUM(hint_descriptor))
                        control->default_value = control->min;

                    else if (LADSPA_IS_HINT_DEFAULT_LOW(hint_descriptor)) {
                        if (is_log)
                            control->default_value = expf(logf(control->min) * 0.75f + logf(control->max) * 0.25f);
                        else
                            control->default_value = control->min * 0.75f + control->max * 0.25f;
                    }

                    else if (LADSPA_IS_HINT_DEFAULT_MIDDLE(hint_descriptor)) {
                        if (is_log)
                            control->default_value = expf(0.5f * (logf(control->min) + logf(control->max)));
                        else
                            control->default_value = 0.5f * (control->min + control->max);
                    }

                    else if (LADSPA_IS_HINT_DEFAULT_HIGH(hint_descriptor)) {
                        if (is_log)
                            control->default_value = expf(logf(control->min) * 0.25f + logf(control->max) * 0.75f);
                        else
                            control->default_value = control->min * 0.25f + control->max * 0.75f;
                    }

                    else if (LADSPA_IS_HINT_DEFAULT_MAXIMUM(hint_descriptor))
                        control->default_value = control->max;
                }

                control->value = control->default_value;
                descriptor->connect_port(instance, port_i, &control->value);
                create.add_control<float>(ctl_in.size(), control->name, control->default_value);
                ctl_in.push_back(std::move(control));
            }

            else if (LADSPA_IS_PORT_OUTPUT(port))
            {
                std::unique_ptr<ControlOutput> control = std::make_unique<ControlOutput>();
                control->name = descriptor->PortNames[port_i];
                control->port_index = port_i;

                descriptor->connect_port(instance, port_i, &control->value);
                ctl_out.push_back(std::move(control));
            }
        }

        else if (LADSPA_IS_PORT_AUDIO(port))
        {
            // input buffer
            if (LADSPA_IS_PORT_INPUT(port))
            {
                float* input_buf = new float[create.engine.frames_per_buffer()];
                input_buffers.push_back(input_buf);
                descriptor->connect_port(instance, port_i, input_buf);
            }

            // output buffer
            else if (LADSPA_IS_PORT_OUTPUT(port))
            {
                float* output_buf = new float[create.engine.frames_per_buffer()];
                output_buffers.push_back(output_buf);
                descriptor->connect_port(instance, port_i, output_buf);
            }
        }
    }
    
    // create audio channels
    if (input_buffers.size() > 0)
    {
        assert(input_buffers.size() <+ UINT8_MAX);
        create.add_audio_input(input_buffers.size());
    }

    if (output_buffers.size() > 0)
    {
        assert(output_buffers.size() <= UINT8_MAX);
        create.add_audio_output(output_buffers.size());
    }

    if (descriptor->run == nullptr)
        logger::log_warning("LV1: descriptor->run == nullptr for %s", descriptor->Name);

    if (descriptor->activate != nullptr)
        descriptor->activate(instance);
}

Lv1Module::~Lv1Module()
{
    if (descriptor->deactivate != nullptr)
        descriptor->deactivate(instance);

    if (descriptor->cleanup == nullptr)
        logger::log_warning("LV1: descriptor->cleanup == nullptr for %s", descriptor->Name);
    else
        descriptor->cleanup(instance);
    
    for (float *buf : input_buffers)
        delete[] buf;

    for (float *buf : output_buffers)
        delete[] buf;
}

void Lv1Module::process(modules::ModuleProcessor &proc)
{
    uint8_t input_channels = proc.audio_input_channels(0);
    uint8_t output_channels = proc.audio_output_channels(0);

    assert(input_buffers.size() == input_channels);
    assert(output_buffers.size() == output_channels);

    // update controls
    for (unsigned int i = 0; i < ctl_in.size(); i++)
    {
        auto &ctl = ctl_in[i];
        ctl->value = proc.get_control_value<float>(i);
    }

    // update input buffers
    if (input_channels > 0)
    {
        float *in = proc.audio_input(0);
        float **in_bufs = input_buffers.data();
        assert(in != nullptr);

        for (unsigned int i = 0; i < proc.buffer_frame_count; i++)
        {
            for (uint8_t c = 0; c < input_channels; c++)
                in_bufs[c][i] = *in++;
        }
    }

    if (descriptor->run != nullptr)
        descriptor->run(instance, proc.buffer_frame_count);

    // update output buffers
    if (output_channels > 0)
    {
        float *out = proc.audio_output(0);
        float **out_bufs = output_buffers.data();
        assert(out != nullptr);

        for (unsigned int i = 0; i < proc.buffer_frame_count; i++)
        {
            for (uint8_t c = 0; c < output_channels; c++)
                *out++ = out_bufs[c][i];
        }
    }
}

void Lv1Module::ui()
{
    unsigned int id = 0;

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));

    for (auto &in : ctl_in)
    {
        ImGui::PushID(id);

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(in->name.c_str());
        ImGui::SameLine(ImGui::GetFontSize() * 12.0f);

        if (in->flags & INPUT_TOGGLE)
        {
            bool toggle = get_control_value<float>(id) > 0.0f;
            if (ImGui::Checkbox("##checkbox", &toggle))
            {
                set_control_value<float>(id, toggle ? 1.0f : 0.0f);
            }
        }
        else
        {
            ImGuiSliderFlags flags = 0;
            const char *format = "%.3f";

            if (in->flags & INPUT_LOGARITHMIC)
                flags |= ImGuiSliderFlags_Logarithmic;
            if (in->flags & INPUT_INTEGER)
                format = "%.0f";

            float v = get_control_value<float>(id);
            bool changed = ImGui::SliderFloat("##slider", &v, in->min, in->max, format, flags);
            if ((in->flags & INPUT_HAS_DEFAULT) != 0 && ImGui::IsItemClicked(ImGuiMouseButton_Middle))
            {
                v = in->default_value;
                changed = true;
            }

            if (changed) set_control_value<float>(id, v);
        }

        ImGui::PopID();

        id++;
    }

    ImGui::PopStyleVar();
}