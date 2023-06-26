#include <cassert>
#include <cstring>
#include <iostream>

#include "lv2.h"

using namespace plugins;

static LilvWorld* LILV_WORLD = nullptr;

void Lv2Plugin::lilv_init() {
    LILV_WORLD = lilv_world_new();
}

void Lv2Plugin::lilv_fini() {
    lilv_world_free(LILV_WORLD);
}

const char* Lv2Plugin::get_standard_paths()
{
    const char* list_str = std::getenv("LV2_PATH");
    if (list_str == nullptr)
        list_str =
#ifdef __APPLE__
            "~/.lv2:~/Library/Audio/Plug-Ins/LV2:/usr/local/lib/lv2:/usr/lib/lv2:/Library/Audio/Plug-Ins/LV2";
#elif defined(_WIN32)
            "%APPDATA%\\LV2;%COMMONPROGRAMFILES%\\LV2"
#else
            "~/.lv2:/usr/local/lib/lv2:/usr/lib/lv2";
#endif
    // haiku paths are defined in lilv source files as "~/.lv2:/boot/common/add-ons/lv2"

    return list_str;
}

void Lv2Plugin::scan_plugins(const std::vector<std::string> &paths, std::vector<PluginData> &data_out)
{
    assert(LILV_WORLD);

    // create path string to be parsed by lilv
    const char delim =
#ifdef _WIN32
        ';';
#else
        ':';
#endif
    std::string path_str;

    for (int i = 0; i < paths.size(); i++) {
        if (i != 0) path_str += delim;
        path_str += paths[i];
    }

    LilvNode* lv2_path = lilv_new_file_uri(LILV_WORLD, NULL, "/usr/lib/lv2");
    lilv_world_set_option(LILV_WORLD, LILV_OPTION_LV2_PATH, lv2_path);
    lilv_node_free(lv2_path);

    lilv_world_load_all(LILV_WORLD);

    const LilvPlugins* plugins = lilv_world_get_all_plugins(LILV_WORLD);

    // list plugins
    LILV_FOREACH (plugins, i, plugins)
    {
        const LilvPlugin* plug = lilv_plugins_get(plugins, i);

        PluginData data;
        data.type = PluginType::Lv2;

        const LilvNode* uri_node = lilv_plugin_get_uri(plug);
        data.id = "plugin.lv2:";
        data.id += lilv_node_as_uri(uri_node);

        const LilvNode* lib_uri = lilv_plugin_get_library_uri(plug);
        char* file_path = lilv_node_get_path(lib_uri, NULL);
        data.file_path = file_path;

        LilvNode* name = lilv_plugin_get_name(plug);
        if (name) {
            data.name = lilv_node_as_string(name);
            lilv_node_free(name);
        } else {
            data.name = lilv_node_as_uri(uri_node);
        }
        
        LilvNode* author = lilv_plugin_get_author_name(plug);
        if (author) {
            data.author = lilv_node_as_string(author);
            lilv_node_free(author);
        }

        const LilvPluginClass* plug_class = lilv_plugin_get_class(plug);
        const char* class_uri = lilv_node_as_uri ( lilv_plugin_class_get_uri(plug_class) );
        data.is_instrument = strcmp(class_uri, LV2_CORE__InstrumentPlugin) == 0;
        data_out.push_back(data);
    }
}




// plugin instances
Lv2Plugin::Lv2Plugin(audiomod::DestinationModule& dest, const PluginData& plugin_data)
    : Plugin(plugin_data), dest(dest)
{
    if (plugin_data.type != PluginType::Lv2)
        throw std::runtime_error("mismatched plugin types");

    // get plugin uri
    LilvNode* plugin_uri; {
        size_t colon_sep = plugin_data.id.find_first_of(":");
        const char* uri_str = plugin_data.id.c_str() + colon_sep + 1;
        plugin_uri = lilv_new_uri(LILV_WORLD, uri_str);
    }

    // get plugin
    const LilvPlugins* plugins = lilv_world_get_all_plugins(LILV_WORLD);
    const LilvPlugin* plugin = lilv_plugins_get_by_uri(plugins, plugin_uri);
    lilv_node_free(plugin_uri);

    if (plugin == nullptr) {
        throw lv2_error("plugin not found");
    }

    // instantiate plugin
    instance = lilv_plugin_instantiate(plugin, dest.sample_rate, NULL);
    if (instance == nullptr) {
        std::cerr << "instantiation failed\n";
        throw lv2_error("instantiation failed");
    }

    // create and connect ports
    const uint32_t port_count = lilv_plugin_get_num_ports(plugin);

    // get port ranges
    float* min_values = new float[port_count];
    float* max_values = new float[port_count];
    float* default_values = new float[port_count];
    lilv_plugin_get_port_ranges_float(plugin, min_values, max_values, default_values);

    // define uris
    LilvNode* lv2_InputPort = lilv_new_uri(LILV_WORLD, LV2_CORE__InputPort);
    LilvNode* lv2_OutputPort = lilv_new_uri(LILV_WORLD, LV2_CORE__OutputPort);
    LilvNode* lv2_AudioPort = lilv_new_uri(LILV_WORLD, LV2_CORE__AudioPort);
    LilvNode* lv2_ControlPort = lilv_new_uri(LILV_WORLD, LV2_CORE__ControlPort);
    LilvNode* lv2_connectionOptional = lilv_new_uri(LILV_WORLD, LV2_CORE__connectionOptional);

    for (uint32_t i = 0; i < port_count; i++)
    {
        const LilvPort* port = lilv_plugin_get_port_by_index(plugin, i);
        
        LilvNode* name_node = lilv_port_get_name(plugin, port);
        const char* port_name = lilv_node_as_string(name_node);

        // input control port
        if (lilv_port_is_a(plugin, port, lv2_ControlPort) && lilv_port_is_a(plugin, port, lv2_InputPort))
        {
            ControlInput* ctl = new ControlInput;
            ctl->name = port_name;
            ctl->port_handle = port;
            ctl->min = min_values[i];
            ctl->max = max_values[i];
            ctl->default_value = default_values[i];
            ctl->value = ctl->default_value;
            ctl->is_integer = false;
            ctl->is_logarithmic = false;
            ctl->is_sample_rate = false;
            ctl->is_toggle = false;
            ctl_in.push_back(ctl);

            lilv_instance_connect_port(instance, i, &ctl->value);
        }

        // output control port
        else if (lilv_port_is_a(plugin, port, lv2_ControlPort) && lilv_port_is_a(plugin, port, lv2_OutputPort))
        {
            ControlOutput* ctl = new ControlOutput;
            ctl->name = port_name;
            ctl->port_handle = port;

            lilv_instance_connect_port(instance, i, &ctl->value);
        }

        // an unsupported port type, throw an error if this port is required
        else if (!lilv_port_has_property(plugin, port, lv2_connectionOptional))
        {
            throw lv2_error("unsupported port type");
        }

        lilv_node_free(name_node);
    }

    delete[] min_values;
    delete[] max_values;
    delete[] default_values;
}

Lv2Plugin::~Lv2Plugin()
{
    lilv_instance_free(instance);
}

// TODO
int Lv2Plugin::control_value_count() const
{
    return 0;
}

// TODO
int Lv2Plugin::output_value_count() const
{
    return 0;
}

// TODO
Plugin::ControlValue Lv2Plugin::get_control_value(int index)
{
    return {};
}

// TODO
Plugin::OutputValue Lv2Plugin::get_output_value(int index)
{
    return {};
}

// TODO
void Lv2Plugin::start()
{}

// TODO
void Lv2Plugin::stop()
{}

// TODO
void Lv2Plugin::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size)
{
    for (size_t i = 0; i < buffer_size; i += 2)
    {
        output[i] = 0.0f;
        output[i + 1] = 0.0f;
        
        for (size_t j = 0; j < num_inputs; j++)
        {
            output[i] += inputs[j][i];
            output[i + 1] += inputs[j][i + 1];
        }
    }
}

// TODO
void Lv2Plugin::save_state(std::ostream& ostream) const
{
    return;
}

// TODO
bool Lv2Plugin::load_state(std::istream& istream, size_t size)
{
    return false;
}