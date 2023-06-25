#include <iostream>
#include <filesystem>
#include <cmath>
#include "ladspa.h"

using namespace plugins;

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
    : Plugin(plugin_data), dest(dest)
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

    // connect ports
    for (int port_i = 0; port_i < descriptor->PortCount; port_i++)
    {
        LADSPA_PortDescriptor port = descriptor->PortDescriptors[port_i];

        if (LADSPA_IS_PORT_INPUT(port) && LADSPA_IS_PORT_CONTROL(port))
        {
            ControlValue* control = new ControlValue;
            control_values.push_back(control);
            control->name = descriptor->PortNames[port_i];
            
            // use these values if unspecified
            control->min = -1.0f;
            control->default_value = 0.0f;
            control->has_default = false;
            control->max = 1.0f;
            
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
                if (LADSPA_IS_HINT_DEFAULT_0(hint_descriptor))
                    control->default_value = 0.0f;

                if (LADSPA_IS_HINT_DEFAULT_1(hint_descriptor))
                    control->default_value = 1.0f;

                if (LADSPA_IS_HINT_DEFAULT_100(hint_descriptor))
                    control->default_value = 100.0f;

                if (LADSPA_IS_HINT_DEFAULT_440(hint_descriptor))
                    control->default_value = 440.0f;

                if (LADSPA_IS_HINT_DEFAULT_MINIMUM(hint_descriptor))
                    control->default_value = control->min;

                if (LADSPA_IS_HINT_DEFAULT_LOW(hint_descriptor)) {
                    if (control->is_logarithmic)
                        control->default_value = expf(logf(control->min) * 0.75f + logf(control->max) * 0.25f);
                    else
                        control->default_value = control->min * 0.75f + control->max * 0.25f;
                }

                if (LADSPA_IS_HINT_DEFAULT_MIDDLE(hint_descriptor)) {
                    if (control->is_logarithmic)
                        control->default_value = expf(0.5f * (logf(control->min) + logf(control->max)));
                    else
                        control->default_value = 0.5f * (control->min + control->max);
                }

                if (LADSPA_IS_HINT_DEFAULT_HIGH(hint_descriptor)) {
                    if (control->is_logarithmic)
                        control->default_value = expf(logf(control->min) * 0.25f + logf(control->max) * 0.75f);
                    else
                        control->default_value = control->min * 0.25f + control->max * 0.75f;
                }

                if (LADSPA_IS_HINT_DEFAULT_MAXIMUM(hint_descriptor))
                    control->default_value = control->max;
            }

            control->value = control->default_value;
            descriptor->connect_port(instance, port_i, &control->value);
        }

        if (LADSPA_IS_PORT_AUDIO(port))
        {
            // input buffer
            if (LADSPA_IS_PORT_INPUT(port))
            {
                // dest.buffer_size is frames per buffer
                float* input_buf = new float[dest.buffer_size];
                input_buffers.push_back(input_buf);
                descriptor->connect_port(instance, port_i, input_buf);
            }

            // output buffer
            else if (LADSPA_IS_PORT_OUTPUT(port))
            {
                float* output_buf = new float[dest.buffer_size];
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

    // initialize input buffers
    if (input_buffers.size() > 0)
    {
        // input is mono
        if (input_buffers.size() == 1)
        {
            float* input_buf = input_buffers.front();

            for (size_t i = 0; i < dest.buffer_size; i++)
            {
                input_buf[i] = 0.0f;

                for (size_t j = 0; j < num_inputs; j++)
                    input_buf[i] += inputs[j][i * 2] + inputs[j][i * 2 + 1];
            }
        }

        // input is stereo
        else if (input_buffers.size() == 2)
        {
            for (size_t i = 0; i < dest.buffer_size; i++)
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
            for (float* buf : input_buffers)
                for (size_t i = 0; i < dest.buffer_size; i++)
                    buf[i] = 0.0f;
        }
    }

    descriptor->run(instance, dest.buffer_size);

    // write output buffers
    // mono
    if (output_buffers.size() == 1)
    {
        float* buf = output_buffers.front();
        for (size_t i = 0; i < dest.buffer_size; i++) {
            output[i * 2] = buf[i];
            output[i * 2 + 1] = buf[i];
        }
    }

    // stereo
    else if (output_buffers.size() == 2)
    {
        for (size_t i = 0; i < dest.buffer_size; i++) {
            output[i * 2] = output_buffers[0][i];
            output[i * 2 + 1] = output_buffers[1][i];
        }
    }
}