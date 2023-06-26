#include <iostream>
#include <filesystem>
#include <cstring>
#include <cmath>
#include "ladspa.h"
#include "../sys.h"

using namespace plugins;

const char* LadspaPlugin::get_standard_paths()
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

static std::vector<PluginData> get_plugin_data(const char *path)
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

void LadspaPlugin::scan_plugins(const std::vector<std::string>& ladspa_paths, std::vector<PluginData> &plugin_data)
{
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
                std::vector<PluginData> plugins = get_plugin_data(entry.path().string().c_str());

                for (PluginData& plugin : plugins)
                {
                    std::cout << "\t" << plugin.name << " by " << plugin.author << "\n";
                    plugin_data.push_back(plugin);
                }
            }
        }
    }
}

// constructor
LadspaPlugin::LadspaPlugin(audiomod::DestinationModule& dest, const PluginData& plugin_data)
    : Plugin(plugin_data), dest(dest)
{
    if (plugin_data.type != PluginType::Ladspa)
        throw std::runtime_error("mismatched plugin types");
    
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

    // connect ports
    for (int port_i = 0; port_i < descriptor->PortCount; port_i++)
    {
        LADSPA_PortDescriptor port = descriptor->PortDescriptors[port_i];

        if (LADSPA_IS_PORT_CONTROL(port))
        {
            if (LADSPA_IS_PORT_INPUT(port))
            {
                ControlInput* control = new ControlInput;
                ctl_in.push_back(control);
                control->name = descriptor->PortNames[port_i];
                control->port_index = port_i;
                
                // use these values if unspecified
                control->min = -10.0f;
                control->default_value = 0.0f;
                control->has_default = false;
                control->max = 10.0f;
                
                // read hints
                const LADSPA_PortRangeHint& range_hint = descriptor->PortRangeHints[port_i];
                LADSPA_PortRangeHintDescriptor hint_descriptor = range_hint.HintDescriptor;

                control->is_toggle = LADSPA_IS_HINT_TOGGLED(hint_descriptor);
                control->is_logarithmic = LADSPA_IS_HINT_LOGARITHMIC(hint_descriptor);
                control->is_integer = LADSPA_IS_HINT_INTEGER(hint_descriptor);
                control->is_sample_rate = LADSPA_IS_HINT_SAMPLE_RATE(hint_descriptor);

                if (LADSPA_IS_HINT_BOUNDED_BELOW(hint_descriptor))
                    control->min =
                        range_hint.LowerBound *
                        (control->is_sample_rate ? dest.sample_rate : 1) - // if control describes sample rate
                        (control->is_integer ? -0.01f : 0.0f); // avoid floating point rounding errors

                if (LADSPA_IS_HINT_BOUNDED_ABOVE(hint_descriptor))
                    control->max =
                        range_hint.UpperBound *
                        (control->is_sample_rate ? dest.sample_rate : 1) + // if control describes sample rate
                        (control->is_integer ? 0.01f : 0.0f); // avoid floating point rounding errors

                // default value
                if (LADSPA_IS_HINT_HAS_DEFAULT(hint_descriptor))
                {
                    control->has_default = true;

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
                        if (control->is_logarithmic)
                            control->default_value = expf(logf(control->min) * 0.75f + logf(control->max) * 0.25f);
                        else
                            control->default_value = control->min * 0.75f + control->max * 0.25f;
                    }

                    else if (LADSPA_IS_HINT_DEFAULT_MIDDLE(hint_descriptor)) {
                        if (control->is_logarithmic)
                            control->default_value = expf(0.5f * (logf(control->min) + logf(control->max)));
                        else
                            control->default_value = 0.5f * (control->min + control->max);
                    }

                    else if (LADSPA_IS_HINT_DEFAULT_HIGH(hint_descriptor)) {
                        if (control->is_logarithmic)
                            control->default_value = expf(logf(control->min) * 0.25f + logf(control->max) * 0.75f);
                        else
                            control->default_value = control->min * 0.25f + control->max * 0.75f;
                    }

                    else if (LADSPA_IS_HINT_DEFAULT_MAXIMUM(hint_descriptor))
                        control->default_value = control->max;
                }

                control->value = control->default_value;
                descriptor->connect_port(instance, port_i, &control->value);
            }

            else if (LADSPA_IS_PORT_OUTPUT(port))
            {
                ControlOutput* value = new ControlOutput;
                value->name = descriptor->PortNames[port_i];
                value->port_index = port_i;
                ctl_out.push_back(value);

                descriptor->connect_port(instance, port_i, &value->value);
            }
        }

        else if (LADSPA_IS_PORT_AUDIO(port))
        {
            // input buffer
            if (LADSPA_IS_PORT_INPUT(port))
            {
                float* input_buf = new float[dest.frames_per_buffer * 2];
                input_buffers.push_back(input_buf);
                descriptor->connect_port(instance, port_i, input_buf);
            }

            // output buffer
            else if (LADSPA_IS_PORT_OUTPUT(port))
            {
                float* output_buf = new float[dest.frames_per_buffer * 2];
                output_buffers.push_back(output_buf);
                descriptor->connect_port(instance, port_i, output_buf);
            }
        }
    }
}

LadspaPlugin::~LadspaPlugin()
{
    for (float* buf : input_buffers)
        delete[] buf;

    for (float* buf : output_buffers)
        delete[] buf;
    
    descriptor->cleanup(instance);
    sys::dl_close(lib);
}

int LadspaPlugin::control_value_count() const {
    return ctl_in.size();
}

Plugin::ControlValue LadspaPlugin::get_control_value(int index)
{
    ControlValue value;
    ControlInput* impl = ctl_in[index];

    value.name = impl->name.c_str();
    value.port_index = impl->port_index;
    value.value = &impl->value;
    value.has_default = impl->has_default;
    value.default_value = impl->default_value;
    value.max = impl->max;
    value.min = impl->min;
    value.is_integer = impl->is_integer;
    value.is_logarithmic = impl->is_logarithmic;
    value.is_sample_rate = impl->is_sample_rate;
    value.is_toggle = impl->is_toggle;

    return value;
}

int LadspaPlugin::output_value_count() const {
    return ctl_out.size();
}

Plugin::OutputValue LadspaPlugin::get_output_value(int index)
{
    OutputValue value;
    ControlOutput* impl = ctl_out[index];

    value.name = impl->name.c_str();
    value.port_index = impl->port_index;
    value.value = &impl->value;

    return value;
}

void LadspaPlugin::start()
{
    if (descriptor->activate != nullptr)
        descriptor->activate(instance);
}

void LadspaPlugin::stop()
{
    if (descriptor->deactivate != nullptr)
        descriptor->deactivate(instance);
}

void LadspaPlugin::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size)
{
    if (descriptor->run == nullptr) return;
    bool interleave = false; // TODO add ui checkbox to toggle this

    size_t sample_count;

    // initialize input buffers
    if (input_buffers.size() > 0)
    {
        // send interleaved buffer
        if (input_buffers.size() == 1)
        {
            if (interleave)
            {
                sample_count = buffer_size;
                float* input_buf = input_buffers.front();

                for (size_t i = 0; i < buffer_size; i++)
                {
                    input_buf[i] = 0.0f;
                    input_buf[i + 1] = 0.0f;

                    for (size_t j = 0; j < num_inputs; j++) {
                        input_buf[i] += inputs[j][i];
                        input_buf[i + 1] += inputs[j][i + 1];
                    }
                }
            }
            else // convert to mono
            {
                sample_count = dest.frames_per_buffer;
                float* input_buf = input_buffers.front();

                for (size_t i = 0; i < dest.frames_per_buffer; i++)
                {
                    input_buf[i] = 0.0f;

                    for (size_t j = 0; j < num_inputs; j++)
                        input_buf[i] += inputs[j][i * 2] + inputs[j][i * 2 + 1];
                }
            }
        }

        // separate each channel
        else if (input_buffers.size() == 2)
        {
            sample_count = dest.frames_per_buffer;

            for (size_t i = 0; i < dest.frames_per_buffer; i++)
            {
                input_buffers[0][i] = 0.0f;
                input_buffers[1][i] = 0.0f;

                for (size_t j = 0; j < num_inputs; j++) {
                    input_buffers[0][i] = inputs[j][i * 2];
                    input_buffers[1][i] = inputs[j][i * 2 + 1];
                }
            }
        }

        // unsupported channel count, initialize buffers to 0
        else
        {
            sample_count = dest.frames_per_buffer;

            for (float* buf : input_buffers)
                for (size_t i = 0; i < dest.frames_per_buffer; i++)
                    buf[i] = 0.0f;
        }
    }

    descriptor->run(instance, sample_count);

    // write output buffers
    // mono
    if (output_buffers.size() == 1)
    {
        float* buf = output_buffers.front();

        if (interleave) {
            memcpy(output, buf, buffer_size);
        } else {
            for (size_t i = 0; i < dest.frames_per_buffer; i++) {
                output[i * 2] = buf[i];
                output[i * 2 + 1] = buf[i];
            }
        }
    }

    // stereo
    else if (output_buffers.size() == 2)
    {
        for (size_t i = 0; i < dest.frames_per_buffer; i++) {
            output[i * 2] = output_buffers[0][i];
            output[i * 2 + 1] = output_buffers[1][i];
        }
    }
}

void LadspaPlugin::save_state(std::ostream& stream) const
{
    // LV1
    push_bytes<uint8_t>(stream, (uint8_t) 1);

    // write size of state (used for validation)
    push_bytes<uint32_t>(stream, ctl_in.size() * 4);

    // write list of control values
    for (const ControlInput* control : ctl_in)
    {
        push_bytes<float>(stream, control->value);
    }
}

bool LadspaPlugin::load_state(std::istream& stream, size_t size)
{
    // check LV1
    if (pull_bytesr<uint8_t>(stream) != 1) return false;

    // check size of state
    uint32_t state_size = pull_bytesr<uint32_t>(stream);
    if (ctl_in.size() * 4 != state_size) return false;

    // read list of control values
    for (ControlInput* control : ctl_in)
    {
        control->value = pull_bytesr<float>(stream); 
    }

    return true;
}