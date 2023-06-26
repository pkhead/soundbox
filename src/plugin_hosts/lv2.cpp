#include <cassert>
#include <cstring>
#include <string>
#include <iostream>
#include <unordered_map>

#include "lv2.h"
#include "lv2/units/units.h"
#include "lv2/urid/urid.h"
#include "lv2/log/log.h"
#include "lv2/atom/atom.h"
#include "lv2/atom/util.h"
#include "lv2/midi/midi.h"
#include "../util.h"

using namespace plugins;

static LilvWorld* LILV_WORLD = nullptr;

struct {
    LilvNode* lv2_InputPort;
    LilvNode* lv2_OutputPort;
    LilvNode* lv2_AudioPort;
    LilvNode* lv2_ControlPort;
    LilvNode* lv2_designation;
    LilvNode* lv2_control;
    LilvNode* lv2_connectionOptional;

    LilvNode* atom_AtomPort;
    LilvNode* atom_bufferType;
    LilvNode* atom_Double;
    LilvNode* atom_Sequence;
    LilvNode* atom_supports;

    LilvNode* midi_MidiEvent;

    LilvNode* units_unit;
    LilvNode* units_db;
} static LV2_URIS;

void Lv2Plugin::lilv_init() {
    LILV_WORLD = lilv_world_new();

    LV2_URIS.lv2_InputPort = lilv_new_uri(LILV_WORLD, LV2_CORE__InputPort);
    LV2_URIS.lv2_OutputPort = lilv_new_uri(LILV_WORLD, LV2_CORE__OutputPort);
    LV2_URIS.lv2_AudioPort = lilv_new_uri(LILV_WORLD, LV2_CORE__AudioPort);
    LV2_URIS.lv2_ControlPort = lilv_new_uri(LILV_WORLD, LV2_CORE__ControlPort);
    LV2_URIS.lv2_designation = lilv_new_uri(LILV_WORLD, LV2_CORE__designation);
    LV2_URIS.lv2_control = lilv_new_uri(LILV_WORLD, LV2_CORE__control);
    LV2_URIS.lv2_connectionOptional = lilv_new_uri(LILV_WORLD, LV2_CORE__connectionOptional);

    LV2_URIS.atom_AtomPort = lilv_new_uri(LILV_WORLD, LV2_ATOM__AtomPort);
    LV2_URIS.atom_bufferType = lilv_new_uri(LILV_WORLD, LV2_ATOM__bufferType);
    LV2_URIS.atom_Double = lilv_new_uri(LILV_WORLD, LV2_ATOM__Double);
    LV2_URIS.atom_Sequence = lilv_new_uri(LILV_WORLD, LV2_ATOM__Sequence);
    LV2_URIS.atom_supports = lilv_new_uri(LILV_WORLD, LV2_ATOM__supports);

    LV2_URIS.midi_MidiEvent = lilv_new_uri(LILV_WORLD, LV2_MIDI__MidiEvent);

    LV2_URIS.units_unit = lilv_new_uri(LILV_WORLD, LV2_UNITS__unit);
    LV2_URIS.units_db = lilv_new_uri(LILV_WORLD, LV2_UNITS__db);
}

void Lv2Plugin::lilv_fini() {
    lilv_world_free(LILV_WORLD);
}

// URID EXTENSION //
static std::vector<std::string> URI_MAP;

static LV2_URID uri_map(LV2_URID_Map_Handle handle, const char* uri)
{
    LV2_URID id;
    for (id = 0; id < URI_MAP.size(); id++) {
        if (URI_MAP[id] == uri)
            return id;
    }

    URI_MAP.push_back(uri);
    return id;
}

static const char* uri_unmap(LV2_URID_Map_Handle handle, LV2_URID urid)
{
    if (urid >= URI_MAP.size())
        return nullptr;

    return URI_MAP[urid].c_str();
}
/////////////////////

// LOGGING EXTENSION //
int log_printf(LV2_Log_Handle handle, LV2_URID type, const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    int res = vprintf(fmt, va);
    va_end(va);
    return res;
}

int log_vprintf(LV2_Log_Handle handle, LV2_URID type, const char* fmt, va_list ap)
{
    return vprintf(fmt, ap);
}
///////////////////////

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

        // if it is not specified as an instrument,
        // it is an instrument anyway if it has at least one midi port
        // or if it has no audio inputs
        if (!data.is_instrument) {
            int port_count = lilv_plugin_get_num_ports(plug);
            bool has_audio_input = false;

            for (int i = 0; i < port_count; i++) {
                const LilvPort* port = lilv_plugin_get_port_by_index(plug, i);

                if (
                    lilv_port_is_a(plug, port, LV2_URIS.lv2_InputPort) &&
                    lilv_port_is_a(plug, port, LV2_URIS.atom_AtomPort) &&
                    lilv_node_equals(lilv_port_get(plug, port, LV2_URIS.atom_bufferType), LV2_URIS.atom_Sequence) &&
                    lilv_node_equals(lilv_port_get(plug, port, LV2_URIS.atom_supports), LV2_URIS.midi_MidiEvent) 
                ) {
                    data.is_instrument = true;
                    break;
                }

                if (
                    lilv_port_is_a(plug, port, LV2_URIS.lv2_InputPort) &&
                    lilv_port_is_a(plug, port, LV2_URIS.lv2_AudioPort)
                ) {
                    has_audio_input = true;
                }
            }

            if (!has_audio_input)
                data.is_instrument = true;
        }

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

    // init features
    map = {nullptr, uri_map};
    map_feature = {LV2_URID__map, &map};
    unmap = {nullptr, uri_unmap};
    unmap_feature = {LV2_URID__unmap, &unmap};
    log = {nullptr, log_printf, log_vprintf};
    log_feature = {LV2_LOG_URI, &log};

    // instantiate plugin
    instance = lilv_plugin_instantiate(plugin, dest.sample_rate, features);
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

    for (uint32_t i = 0; i < port_count; i++)
    {
        const LilvPort* port = lilv_plugin_get_port_by_index(plugin, i);
        
        LilvNode* name_node = lilv_port_get_name(plugin, port);
        const char* port_name = name_node ? lilv_node_as_string(name_node) : "[unnamed]";

        bool is_input_port = lilv_port_is_a(plugin, port, LV2_URIS.lv2_InputPort);
        bool is_output_port = lilv_port_is_a(plugin, port, LV2_URIS.lv2_OutputPort);

        LilvNode* designation_n = lilv_port_get(plugin, port, LV2_URIS.lv2_designation);

        // input control port
        if (
            lilv_port_is_a(plugin, port, LV2_URIS.lv2_ControlPort) &&
            is_input_port
        ) {
            ControlInput* ctl = new ControlInput;
            ctl->name = port_name;
            ctl->port_handle = port;
            ctl->min = min_values[i];
            ctl->max = max_values[i];
            ctl->default_value = default_values[i];
            ctl->value = ctl->default_value;
            ctl->unit = UnitType::None;

            LilvNode* unit_n = lilv_port_get(plugin, port, LV2_URIS.units_unit);
            
            if (unit_n) {
                // TODO: more units
                if (lilv_node_equals(unit_n, LV2_URIS.units_db))
                    ctl->unit = UnitType::dB;
                 
                lilv_node_free(unit_n);
            }

            ctl_in.push_back(ctl);

            lilv_instance_connect_port(instance, i, &ctl->value);
        }

        // output control port
        else if (
            lilv_port_is_a(plugin, port, LV2_URIS.lv2_ControlPort) &&
            is_output_port
        ) {
            ControlOutput* ctl = new ControlOutput;
            ctl->name = port_name;
            ctl->port_handle = port;

            lilv_instance_connect_port(instance, i, &ctl->value);
        }

        // input audio port
        else if (
            lilv_port_is_a(plugin, port, LV2_URIS.lv2_AudioPort) &&
            is_input_port
        ) {
            float* input_buf = new float[dest.frames_per_buffer * 2];
            audio_input_bufs.push_back(input_buf);
            lilv_instance_connect_port(instance, i, input_buf);
        }

        // output audio port
        else if (
            lilv_port_is_a(plugin, port, LV2_URIS.lv2_AudioPort) &&
            is_output_port
        ) {
            float* output_buf = new float[dest.frames_per_buffer * 2];
            audio_output_bufs.push_back(output_buf);
            lilv_instance_connect_port(instance, i, output_buf);
        }

        // input atom port
        else if (
            lilv_port_is_a(plugin, port, LV2_URIS.atom_AtomPort) &&
            is_input_port
        ) {
            LilvNode* buffer_type_n = lilv_port_get(plugin, port, LV2_URIS.atom_bufferType);

            if (buffer_type_n) {
                // atom:Sequence
                if (lilv_node_equals(buffer_type_n, LV2_URIS.atom_Sequence)) {
                    if (midi_in != nullptr)
                        throw lv2_error("unsupported: multiple atom sequences");

                    midi_in = new MidiBuffer {
                        {
                            { sizeof(LV2_Atom_Sequence_Body), uri_map(nullptr, LV2_ATOM__Sequence) },
                            { 0, 0 }
                        }
                    };

                    lilv_instance_connect_port(instance, i, midi_in);
                }

                else
                    throw lv2_error(std::string("unsupported atom type: ") + lilv_node_as_uri(buffer_type_n) );
            } else {
                throw lv2_error("atom port with unspecified type");
            }

            lilv_node_free(buffer_type_n);
        }

        // an unsupported port type, throw an error if this port is required
        else if (!lilv_port_has_property(plugin, port, LV2_URIS.lv2_connectionOptional))
        {
            const LilvNodes* class_nodes = lilv_port_get_classes(plugin, port);

            // print node types
            LILV_FOREACH (nodes, i, class_nodes) {
                dbg("%s\n", lilv_node_as_string(lilv_nodes_get(class_nodes, i)));
            }

            throw lv2_error("unsupported port type");
        }

        if (name_node)      lilv_node_free(name_node);
        if (designation_n)  lilv_node_free(designation_n);
    }

    input_combined = new float[dest.frames_per_buffer * 2];

    delete[] min_values;
    delete[] max_values;
    delete[] default_values;
}

Lv2Plugin::~Lv2Plugin()
{
    lilv_instance_free(instance);
    delete[] input_combined;

    for (float* buf : audio_input_bufs)
        delete buf;

    for (float* buf : audio_output_bufs)
        delete buf;

    for (ControlInput* in : ctl_in)
        delete in;

    for (ControlOutput* out : ctl_out)
        delete out;

    if (midi_in) delete[] midi_in;
    if (midi_out) delete[] midi_out;
}

int Lv2Plugin::control_value_count() const
{
    return ctl_in.size();
}

int Lv2Plugin::output_value_count() const
{
    return ctl_out.size();
}

Plugin::ControlValue Lv2Plugin::get_control_value(int index)
{
    ControlValue value;
    ControlInput* impl = ctl_in[index];

    value.name = impl->name.c_str();
    value.port_index = impl->port_index;
    value.value = &impl->value;
    value.has_default = true;
    value.default_value = impl->default_value;
    value.max = impl->max;
    value.min = impl->min;
    value.is_integer = false;
    value.is_logarithmic = false;
    value.is_sample_rate = false;
    value.is_toggle = false;

    switch (impl->unit)
    {
        case UnitType::dB:
            value.format = "%.3f dB";
            break;

        default:
            value.format = "%.3f";
            break;
    }
    
    return value;
}

Plugin::OutputValue Lv2Plugin::get_output_value(int index)
{
    OutputValue value;
    ControlOutput* impl = ctl_out[index];

    value.name = impl->name.c_str();
    value.format = "%.3f";
    value.port_index = impl->port_index;
    value.value = &impl->value;

    return value;
}

void Lv2Plugin::start()
{
    lilv_instance_activate(instance);
}

void Lv2Plugin::stop()
{
    lilv_instance_deactivate(instance);
}

void Lv2Plugin::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size)
{
    bool interleave = false; // TODO add ui checkbox to toggle this

    // initialize input buffers
    for (size_t i = 0; i < buffer_size; i += 2) {
        input_combined[i] = 0.0f;
        input_combined[i + 1] = 0.0f;

        for (size_t j = 0; j < num_inputs; j++) {
            input_combined[i] += inputs[j][i];
            input_combined[i + 1] += inputs[j][i + 1];
        }
    }

    size_t sample_count = convert_from_stereo(
        input_combined,
        &audio_input_bufs.front(),
        audio_input_bufs.size(),
        dest.frames_per_buffer,
        interleave
    );

    lilv_instance_run(instance, sample_count);

    // write output buffers
    convert_to_stereo(
        &audio_output_bufs.front(),
        output,
        audio_output_bufs.size(),
        dest.frames_per_buffer,
        interleave
    );
}

void Lv2Plugin::event(uint64_t timestamp, const audiomod::MidiEvent* midi_event)
{
    if (midi_in)
    {
        struct {
            LV2_Atom_Event header;
            audiomod::MidiEvent midi;
        } atom;

        atom.header.time.frames = timestamp;
        atom.header.body.size = midi_event->size();
        atom.header.body.type = uri_map(nullptr, LV2_MIDI__MidiEvent);
        memcpy(&atom.midi, midi_event, midi_event->size());

        lv2_atom_sequence_append_event(&midi_in->header, MIDI_BUFFER_CAPACITY, &atom.header);
    }
}

std::vector<audiomod::MidiEvent> Lv2Plugin::receive_events()
{
    return std::vector<audiomod::MidiEvent>();
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