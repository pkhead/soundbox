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
#include "lv2/atom/forge.h"
#include "lv2/atom/util.h"
#include "lv2/midi/midi.h"
#include "lv2/time/time.h"
#include "lv2/patch/patch.h"
#include "lv2/state/state.h"
#include "../util.h"
#include "../song.h"

using namespace plugins;

static LilvWorld* LILV_WORLD = nullptr;

struct {
    LilvNode* a;
    LilvNode* rdfs_label;
    LilvNode* rdfs_range;

    LilvNode* lv2_InputPort;
    LilvNode* lv2_OutputPort;
    LilvNode* lv2_AudioPort;
    LilvNode* lv2_ControlPort;
    LilvNode* lv2_designation;
    LilvNode* lv2_control;
    LilvNode* lv2_connectionOptional;
    LilvNode* lv2_Parameter;

    LilvNode* atom_AtomPort;
    LilvNode* atom_bufferType;
    LilvNode* atom_Double;
    LilvNode* atom_Sequence;
    LilvNode* atom_supports;

    LilvNode* midi_MidiEvent;

    LilvNode* time_Position;

    LilvNode* patch_Message;
    LilvNode* patch_writable;
    LilvNode* patch_readable;

    LilvNode* state_interface;
    LilvNode* state_loadDefaultState;

    LilvNode* units_unit;
    LilvNode* units_db;
} static URI;

#define RDFS_PREFIX "http://www.w3.org/2000/01/rdf-schema#"

void Lv2Plugin::lilv_init() {
    LILV_WORLD = lilv_world_new();

    URI.a = lilv_new_uri(LILV_WORLD, "http://www.w3.org/1999/02/22-rdf-syntax-ns#type");
    URI.rdfs_label = lilv_new_uri(LILV_WORLD, RDFS_PREFIX "label");
    URI.rdfs_range = lilv_new_uri(LILV_WORLD, RDFS_PREFIX "range");

    URI.lv2_InputPort = lilv_new_uri(LILV_WORLD, LV2_CORE__InputPort);
    URI.lv2_OutputPort = lilv_new_uri(LILV_WORLD, LV2_CORE__OutputPort);
    URI.lv2_AudioPort = lilv_new_uri(LILV_WORLD, LV2_CORE__AudioPort);
    URI.lv2_ControlPort = lilv_new_uri(LILV_WORLD, LV2_CORE__ControlPort);
    URI.lv2_designation = lilv_new_uri(LILV_WORLD, LV2_CORE__designation);
    URI.lv2_control = lilv_new_uri(LILV_WORLD, LV2_CORE__control);
    URI.lv2_connectionOptional = lilv_new_uri(LILV_WORLD, LV2_CORE__connectionOptional);
    URI.lv2_Parameter = lilv_new_uri(LILV_WORLD, LV2_CORE_PREFIX "Parameter");

    URI.atom_AtomPort = lilv_new_uri(LILV_WORLD, LV2_ATOM__AtomPort);
    URI.atom_bufferType = lilv_new_uri(LILV_WORLD, LV2_ATOM__bufferType);
    URI.atom_Double = lilv_new_uri(LILV_WORLD, LV2_ATOM__Double);
    URI.atom_Sequence = lilv_new_uri(LILV_WORLD, LV2_ATOM__Sequence);
    URI.atom_supports = lilv_new_uri(LILV_WORLD, LV2_ATOM__supports);

    URI.midi_MidiEvent = lilv_new_uri(LILV_WORLD, LV2_MIDI__MidiEvent);

    URI.time_Position = lilv_new_uri(LILV_WORLD, LV2_TIME__Position);

    URI.patch_Message = lilv_new_uri(LILV_WORLD, LV2_PATCH__Message);
    URI.patch_writable = lilv_new_uri(LILV_WORLD, LV2_PATCH__writable);
    URI.patch_readable = lilv_new_uri(LILV_WORLD, LV2_PATCH__readable);

    URI.state_interface = lilv_new_uri(LILV_WORLD, LV2_STATE__interface);
    URI.state_loadDefaultState = lilv_new_uri(LILV_WORLD, LV2_STATE__loadDefaultState);

    URI.units_unit = lilv_new_uri(LILV_WORLD, LV2_UNITS__unit);
    URI.units_db = lilv_new_uri(LILV_WORLD, LV2_UNITS__db);
}

void Lv2Plugin::lilv_fini() {
    lilv_world_free(LILV_WORLD);
}

/**
* A wrapper around a LilvNode* that is
* automatically freed when it goes out of scope
**/
struct LilvNode_ptr
{
private:
    LilvNode* node;
public:
    LilvNode_ptr() : node(nullptr) {};
    LilvNode_ptr(LilvNode* node) : node(node) {};
    ~LilvNode_ptr() {
        if (node)
            lilv_node_free(node);
    }

    inline LilvNode* operator->() const noexcept {
        return node;
    }

    inline operator LilvNode*() const noexcept {
        return node;
    }

    inline operator bool() const noexcept {
        return node;
    }

    inline LilvNode* operator=(LilvNode* new_node) noexcept {
        node = new_node;
        return node;
    }
};

// URID EXTENSION //
static std::vector<std::string> URI_MAP;

static LV2_URID uri_map(const char* uri)
{
    LV2_URID id;
    for (id = 0; id < URI_MAP.size(); id++) {
        if (URI_MAP[id] == uri)
            return id;
    }

    URI_MAP.push_back(uri);
    return id;
}

static const char* uri_unmap(LV2_URID urid)
{
    if (urid >= URI_MAP.size())
        return nullptr;

    return URI_MAP[urid].c_str();
}

static LV2_URID uri_map_handle(LV2_URID_Map_Handle handle, const char* uri)
{
    LV2_URID id;
    for (id = 0; id < URI_MAP.size(); id++) {
        if (URI_MAP[id] == uri)
            return id;
    }

    URI_MAP.push_back(uri);
    return id;
}

static const char* uri_unmap_handle(LV2_URID_Map_Handle handle, LV2_URID urid)
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



















//////////////////////
// Plugin discovery //
//////////////////////

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

    LilvNode_ptr lv2_path = lilv_new_file_uri(LILV_WORLD, NULL, "/usr/lib/lv2");
    lilv_world_set_option(LILV_WORLD, LILV_OPTION_LV2_PATH, lv2_path);

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

        LilvNode_ptr name = lilv_plugin_get_name(plug);
        if (name) {
            data.name = lilv_node_as_string(name);
        } else {
            data.name = lilv_node_as_uri(uri_node);
        }
        
        LilvNode_ptr author = lilv_plugin_get_author_name(plug);
        if (author) {
            data.author = lilv_node_as_string(author);
        }

        LilvNode_ptr license_uri_n = lilv_new_uri(LILV_WORLD, "http://usefulinc.com/ns/doap#license");

        LilvNode_ptr license_n = lilv_world_get(LILV_WORLD, uri_node, license_uri_n, NULL);
        if (license_n) {
            data.copyright = lilv_node_as_string(license_n);
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
                    lilv_port_is_a(plug, port, URI.lv2_InputPort) &&
                    lilv_port_is_a(plug, port, URI.atom_AtomPort) &&
                    lilv_node_equals(lilv_port_get(plug, port, URI.atom_bufferType), URI.atom_Sequence) &&
                    lilv_node_equals(lilv_port_get(plug, port, URI.atom_supports), URI.midi_MidiEvent) 
                ) {
                    data.is_instrument = true;
                    break;
                }

                if (
                    lilv_port_is_a(plug, port, URI.lv2_InputPort) &&
                    lilv_port_is_a(plug, port, URI.lv2_AudioPort)
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




















//////////////////////////
// Plugin instantiation //
//////////////////////////

Lv2Plugin::Parameter::Parameter(const char* urid, const char* label, const char* type_uri)
    : id(uri_map(urid)),
      label(label)
{
    if (strcmp(type_uri, LV2_ATOM__Int) == 0) {
        type = TypeInt;
        v._int = 0;
        last_v._int = 0;
    } else if (strcmp(type_uri, LV2_ATOM__Long) == 0) {
        type = TypeLong;
        v._long = 0;
        last_v._long = 0;
    } else if (strcmp(type_uri, LV2_ATOM__Float) == 0) {
        type = TypeFloat;
        v._float = 0;
        last_v._float = 0;
    } else if (strcmp(type_uri, LV2_ATOM__Double) == 0) {
        type = TypeDouble;
        v._double = 0;
        last_v._double = 0;
    } else if (strcmp(type_uri, LV2_ATOM__Bool) == 0) {
        type = TypeBool;
        v._bool = 0;
        last_v._bool = false;
    } else if (strcmp(type_uri, LV2_ATOM__String) == 0) {
        type = TypeString;
    } else if (strcmp(type_uri, LV2_ATOM__Path) == 0) {
        type = TypePath;
    } else {
        throw lv2_error(std::string("unknown type <") + type_uri + ">");
    }
}

const char* Lv2Plugin::Parameter::type_uri() const
{
    switch (type) {
        case TypeInt:
            return LV2_ATOM__Int;
        case TypeLong:
            return LV2_ATOM__Long;
        case TypeFloat:
            return LV2_ATOM__Float;
        case TypeDouble:
            return LV2_ATOM__Double;
        case TypeBool:
            return LV2_ATOM__Bool;
        case TypeString:
            return LV2_ATOM__String;
        case TypePath:
            return LV2_ATOM__Path;
    }

    assert(false);
}

size_t Lv2Plugin::Parameter::size() const {
    switch (type) {
        case TypeInt:
            return sizeof(int);
        case TypeLong:
            return sizeof(long);
        case TypeFloat:
            return sizeof(float);
        case TypeDouble:
            return sizeof(double);
        case TypeBool:
            return sizeof(bool);
        case TypeString:
        case TypePath:
            return str.size();
    }

    return 0;
}

bool Lv2Plugin::Parameter::detect_change()
{
    if (type == TypeString || type == TypePath)
    {
        bool did_change = str != last_str;
        last_str = str;
        return did_change;
    }
    else
    {
        switch (type)
        {
            case TypeFloat: {
                bool did_change = v._float != last_v._float;
                last_v._float = v._float;
                return did_change;
            }

            case TypeDouble: {
                bool did_change = v._double != last_v._double;
                last_v._double = v._double;
                return did_change;
            }

            default: {
                bool did_change = memcmp(&v, &last_v, sizeof(v)) != 0;
                memcpy(&last_v, &v, sizeof(v));

                return did_change;
            }

        }
    }
}

bool Lv2Plugin::Parameter::set(const void* value, uint32_t expected_size, uint32_t expected_type)
{
    if (uri_map(type_uri()) != expected_type) return false;

    if (type == TypeString || type == TypePath) {
        str.resize(expected_size);
        memcpy(str.data(), value, expected_size);
    } else {
        if (size() != expected_size) return false;
        memcpy(&v, value, expected_size);
    }

    return true;
}

const void* Lv2Plugin::Parameter::get(uint32_t* size) const
{
    *size = this->size();

    if (type == TypeString || type == TypePath) {
        return str.c_str();
    } else {
        return &v;
    }
}

Lv2Plugin::Parameter* Lv2Plugin::find_parameter(LV2_URID id) const
{
    for (Parameter* param : parameters) {
        if (param->id == id)
            return param;
    }

    return nullptr;
}

// for use in lilv_state_restore and lilv_state_new_from_instance
void Lv2Plugin::_set_port_value(
    const char* port_symbol,
    const void* value,
    uint32_t size,
    uint32_t type
) {
    // TODO support for types other than float?
    // also check correct type

    for (ControlInputPort* port : ctl_in) {
        if (port->symbol == port_symbol) {
            memcpy(&port->value, value, size);
            break;
        }
    }
}

const void* Lv2Plugin::_get_port_value(
    const char* port_symbol,
    uint32_t* size,
    uint32_t type
) {
    // TODO support for types other than float?
    // also check correct type

    for (ControlInputPort* port : ctl_in) {
        if (port->symbol == port_symbol) {
            *size = sizeof(float);
            return &port->value;
        }
    }

    return nullptr;
}

void Lv2Plugin::set_port_value_callback(
    const char* port_symbol,
    void* user_data,
    const void* value,
    uint32_t size,
    uint32_t type
) {
    Lv2Plugin* plug = (Lv2Plugin*) user_data;
    plug->_set_port_value(port_symbol, value, size, type);
}

const void* Lv2Plugin::get_port_value_callback(
    const char* port_symbol,
    void* user_data,
    uint32_t* size,
    uint32_t type
) {
    Lv2Plugin* plug = (Lv2Plugin*) user_data;
    return plug->_get_port_value(port_symbol, size, type);
}

// plugin instantiation
Lv2Plugin::Lv2Plugin(audiomod::DestinationModule& dest, const PluginData& plugin_data)
    : PluginModule(dest, plugin_data)
{
    if (plugin_data.type != PluginType::Lv2)
        throw std::runtime_error("mismatched plugin types");

    // get plugin uri
    LilvNode_ptr plugin_uri; {
        size_t colon_sep = plugin_data.id.find_first_of(":");
        const char* uri_str = plugin_data.id.c_str() + colon_sep + 1;
        this->plugin_uri = uri_str;
        plugin_uri = lilv_new_uri(LILV_WORLD, uri_str);
    }

    // get plugin
    const LilvPlugins* plugins = lilv_world_get_all_plugins(LILV_WORLD);
    const LilvPlugin* plugin = lilv_plugins_get_by_uri(plugins, plugin_uri);

    if (plugin == nullptr) {
        throw lv2_error("plugin not found");
    }

    // init features
    map = {nullptr, uri_map_handle};
    map_feature = {LV2_URID__map, &map};
    unmap = {nullptr, uri_unmap_handle};
    unmap_feature = {LV2_URID__unmap, &unmap};
    log = {nullptr, log_printf, log_vprintf};
    log_feature = {LV2_LOG__log, &log};

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
    bool unsupported = false;
    lilv_plugin_get_port_ranges_float(plugin, min_values, max_values, default_values);

    for (uint32_t i = 0; i < port_count; i++)
    {
        const LilvPort* port = lilv_plugin_get_port_by_index(plugin, i);
        
        LilvNode_ptr name_node = lilv_port_get_name(plugin, port);
        const LilvNode* symbol_n = lilv_port_get_symbol(plugin, port);
        const char* port_name = name_node ? lilv_node_as_string(name_node) : "[unnamed]";
        const char* port_symbol = lilv_node_as_string(symbol_n);

        bool is_input_port = lilv_port_is_a(plugin, port, URI.lv2_InputPort);
        bool is_output_port = lilv_port_is_a(plugin, port, URI.lv2_OutputPort);
        bool is_optional = lilv_port_has_property(plugin, port, URI.lv2_connectionOptional);

        LilvNode_ptr designation_n = lilv_port_get(plugin, port, URI.lv2_designation);

        /*
        TODO
        lilv_port_get_value(plugin, port, URI.lv2_portProperty)
        pprops:logarithmic
        */

        // input control port
        if (
            lilv_port_is_a(plugin, port, URI.lv2_ControlPort) &&
            is_input_port
        ) {
            ControlInputPort* ctl = new ControlInputPort;
            ctl->name = port_name;
            ctl->symbol = port_symbol;
            ctl->port_handle = port;
            ctl->min = min_values[i];
            ctl->max = max_values[i];
            ctl->default_value = default_values[i];
            ctl->value = ctl->default_value;
            ctl->unit = UnitType::None;

            LilvNode_ptr unit_n = lilv_port_get(plugin, port, URI.units_unit);
            
            if (unit_n) {
                // TODO: more units
                if (lilv_node_equals(unit_n, URI.units_db))
                    ctl->unit = UnitType::dB;
            }

            ctl_in.push_back(ctl);

            lilv_instance_connect_port(instance, i, &ctl->value);

            // add to interface
            InterfaceDisplay display;
            display.is_parameter = false;
            display.read_only = false;
            display.port_in = ctl;
            input_displays.push_back(display);
        }

        // output control port
        else if (
            lilv_port_is_a(plugin, port, URI.lv2_ControlPort) &&
            is_output_port
        ) {
            ControlOutputPort* ctl = new ControlOutputPort;
            ctl->name = port_name;
            ctl->symbol = port_symbol;
            ctl->port_handle = port;

            lilv_instance_connect_port(instance, i, &ctl->value);

            // add to interface
            InterfaceDisplay display;
            display.is_parameter = false;
            display.read_only = true;
            display.port_out = ctl;
            output_displays.push_back(display);
        }

        // input audio port
        else if (
            lilv_port_is_a(plugin, port, URI.lv2_AudioPort) &&
            is_input_port
        ) {
            float* input_buf = new float[dest.frames_per_buffer * 2];
            audio_input_bufs.push_back(input_buf);
            lilv_instance_connect_port(instance, i, input_buf);
        }

        // output audio port
        else if (
            lilv_port_is_a(plugin, port, URI.lv2_AudioPort) &&
            is_output_port
        ) {
            float* output_buf = new float[dest.frames_per_buffer * 2];
            audio_output_bufs.push_back(output_buf);
            lilv_instance_connect_port(instance, i, output_buf);
        }

        // atom port
        else if (lilv_port_is_a(plugin, port, URI.atom_AtomPort)) {
            if (is_input_port || is_output_port) {
                LilvNode_ptr buffer_type_n = lilv_port_get(plugin, port, URI.atom_bufferType);
                LilvNodes* supports_n = lilv_port_get_value(plugin, port, URI.atom_supports);

                bool is_main = lilv_node_equals(designation_n, URI.lv2_control);
                bool midi_support = lilv_nodes_contains(supports_n, URI.midi_MidiEvent);
                bool patch_support = lilv_nodes_contains(supports_n, URI.patch_Message);
                bool time_support = lilv_nodes_contains(supports_n, URI.time_Position);

                // atom:Sequence
                if (lilv_node_equals(buffer_type_n, URI.atom_Sequence)) {
                    AtomSequenceBuffer* seq_buf = new AtomSequenceBuffer {
                        {
                            { sizeof(LV2_Atom_Sequence_Body), uri_map(LV2_ATOM__Sequence) },
                            { 0, 0 }
                        }
                    };

                    // if control designation is not specified, plugin will
                    // use the first midi port found
                    if (is_input_port)
                    {
                        msg_in.push_back(seq_buf);

                        if (midi_support && (midi_in == nullptr || is_main)) {
                            midi_in = seq_buf;
                        }
                        
                        if (time_support && (time_in == nullptr || is_main)) {
                            time_in = seq_buf;
                        }

                        if (patch_support && (patch_in == nullptr || is_main)) {
                            patch_in = seq_buf;
                        }
                    }
                    else if (is_output_port)
                    {
                        msg_out.push_back(seq_buf);

                        if (midi_support && (midi_out == nullptr || is_main)) {
                            midi_out = seq_buf;
                        }

                        // TODO: time out?

                        if (patch_support && (patch_out == nullptr || is_main)) {
                            patch_out = seq_buf;
                        }
                    }

                    lilv_instance_connect_port(instance, i, seq_buf);
                }

                else
                    throw lv2_error(std::string("unsupported atom type: ") + lilv_node_as_uri(buffer_type_n) );
            } else {
                unsupported = true;
            }
        }
        
        // an unsupported port type, throw an error if this port is required
        else if (!lilv_port_has_property(plugin, port, URI.lv2_connectionOptional))
        {
            unsupported = true;
        }

        if (unsupported) {
            const LilvNodes* class_nodes = lilv_port_get_classes(plugin, port);

            // print node types
            LILV_FOREACH (nodes, i, class_nodes) {
                dbg("%s\n", lilv_node_as_string(lilv_nodes_get(class_nodes, i)));
            }

            throw lv2_error("unsupported port type");
        }

    }

    // scan writable parameters
    LilvNodes* patch_writable_n = lilv_world_find_nodes(
        LILV_WORLD,
        plugin_uri,
        URI.patch_writable,
        NULL
    );

    LILV_FOREACH (nodes, iter, patch_writable_n) {
        const LilvNode* property = lilv_nodes_get(patch_writable_n, iter);
        dbg("writable %s\n", lilv_node_as_uri(property));
        
        assert(lilv_world_ask(LILV_WORLD, property, URI.a, NULL));
        assert(lilv_world_ask(LILV_WORLD, property, URI.rdfs_range, NULL));
        assert(lilv_world_ask(LILV_WORLD, property, URI.rdfs_label, NULL));

        LilvNode_ptr label_n = lilv_world_get(LILV_WORLD, property, URI.rdfs_label, NULL);
        LilvNode_ptr type_n = lilv_world_get(LILV_WORLD, property, URI.rdfs_range, NULL);

        Parameter* param = new Parameter(
            lilv_node_as_uri(property),
            lilv_node_as_string(label_n),
            lilv_node_as_uri(type_n)
        );

        param->is_writable = true;
        param->is_readable = false;
        parameters.push_back(param);
    }

    // scan readable parameters
    LilvNodes* patch_readable_n = lilv_world_find_nodes(
        LILV_WORLD,
        plugin_uri,
        URI.patch_readable,
        NULL
    );

    LILV_FOREACH (nodes, iter, patch_readable_n) {
        const LilvNode* property = lilv_nodes_get(patch_readable_n, iter);
        dbg("readable %s\n", lilv_node_as_uri(property));
        
        assert(lilv_world_ask(LILV_WORLD, property, URI.a, URI.lv2_Parameter));
        assert(lilv_world_ask(LILV_WORLD, property, URI.rdfs_range, NULL));
        assert(lilv_world_ask(LILV_WORLD, property, URI.rdfs_label, NULL));

        LilvNode_ptr label_n = lilv_world_get(LILV_WORLD, property, URI.rdfs_label, NULL);
        LilvNode_ptr type_n = lilv_world_get(LILV_WORLD, property, URI.rdfs_range, NULL);

        Parameter* param = find_parameter( uri_map(lilv_node_as_uri(property)) );
        if (param == nullptr) {
            param = new Parameter(
                lilv_node_as_uri(property),
                lilv_node_as_string(label_n),
                lilv_node_as_uri(type_n)
            );
            parameters.push_back(param);
        }

        param->is_readable = true;
    }

    lilv_nodes_free(patch_writable_n);
    lilv_nodes_free(patch_readable_n);

    // load default state
    if (lilv_plugin_has_feature(plugin, URI.state_loadDefaultState)) {
        LilvState* state = lilv_state_new_from_world(LILV_WORLD, &map, plugin_uri);

        if (state) {
            lilv_state_restore(state, instance, set_port_value_callback, this, 0, features);
            
            lilv_state_free(state);
        }
    }

    // add parameters to interface
    for (Parameter* param : parameters) {
        // TODO: parameter types other than float
        if (param->type == Parameter::TypeFloat) {
            InterfaceDisplay display;
            display.is_parameter = true;
            display.param = param;

            // if parameter is read-only
            if (!param->is_writable && param->is_readable) {
                display.read_only = true;
                output_displays.push_back(display);
            } else {
                display.read_only = false;
                input_displays.push_back(display);
            }
        }
    }

    input_combined = new float[dest.frames_per_buffer * 2];

    // create utility atom forge
    lv2_atom_forge_init(&forge, &map);

    delete[] min_values;
    delete[] max_values;
    delete[] default_values;

    _has_interface = control_value_count() > 0;
    start();
}

#define FREE_ARRAY(array) \
    for (auto val : array) \
        delete val;

Lv2Plugin::~Lv2Plugin()
{
    stop();

    lilv_instance_free(instance);
    delete[] input_combined;

    FREE_ARRAY(audio_input_bufs);
    FREE_ARRAY(audio_output_bufs);
    FREE_ARRAY(ctl_in);
    FREE_ARRAY(ctl_out);
    FREE_ARRAY(msg_in);
    FREE_ARRAY(msg_out);
    FREE_ARRAY(parameters);
}

int Lv2Plugin::control_value_count() const
{
    return input_displays.size();
}

int Lv2Plugin::output_value_count() const
{
    return output_displays.size();
}

PluginModule::ControlValue Lv2Plugin::get_control_value(int index)
{
    ControlValue value;
    InterfaceDisplay& display_info = input_displays[index];

    if (display_info.is_parameter) {
        Parameter* param = display_info.param;

        // TODO: parameter types other than float
        value.name = param->label.c_str();
        value.value = &param->v._float;
        value.has_default = false;
        value.min = -10.0f;
        value.max = 10.0f;
        value.is_integer = false;
        value.is_logarithmic = false;
        value.is_sample_rate = false;
        value.is_toggle = false;
        value.format = "%.3f";
    } else {
        ControlInputPort* impl = display_info.port_in;

        value.name = impl->name.c_str();
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
    }

    return value;
}

PluginModule::OutputValue Lv2Plugin::get_output_value(int index)
{
    OutputValue value;
    InterfaceDisplay& display = output_displays[index];

    if (display.is_parameter) {
        Parameter* param = display.param;

        // TODO: parameter types other than float
        value.name = param->label.c_str();
        value.format = "%.3f";
        value.value = &param->v._float;
    } else {
        ControlOutputPort* impl = display.port_out;

        value.name = impl->name.c_str();
        value.format = "%.3f";
        value.value = &impl->value;
    }

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

void Lv2Plugin::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count)
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
        _dest.frames_per_buffer,
        interleave
    );

    // send buffer capacity to plugins
    for (AtomSequenceBuffer* buf : msg_out)
        buf->header.atom.size = ATOM_SEQUENCE_CAPACITY;

    // set and monitor required parameters
    if (patch_in)
    {
        LV2_Atom_Event* event = lv2_atom_sequence_end(&patch_in->header.body, patch_in->header.atom.size);
        lv2_atom_forge_set_buffer(
            &forge,
            (uint8_t*) event,
            ATOM_SEQUENCE_CAPACITY - ((size_t)(event) - (size_t)(patch_in->data))
        );

        LV2_Atom_Forge_Frame frame;

        for (Parameter* param : parameters)
        {
            if (param->is_writable && param->detect_change()) {
                // send request to write parameter
                dbg("%s changed\n", uri_unmap(param->id));
                
                lv2_atom_forge_frame_time(&forge, 0);
                lv2_atom_forge_object(&forge, &frame, 0, uri_map(LV2_PATCH__Set));

                lv2_atom_forge_key(&forge, uri_map(LV2_PATCH__subject));
                lv2_atom_forge_urid(&forge, uri_map(plugin_uri.c_str()));

                lv2_atom_forge_key(&forge, uri_map(LV2_PATCH__property));
                lv2_atom_forge_urid(&forge, param->id);

                uint32_t param_size;
                const void* param_data = param->get(&param_size);

                lv2_atom_forge_key(&forge, uri_map(LV2_PATCH__value));
                lv2_atom_forge_atom(&forge, param_size, uri_map(param->type_uri()));
                lv2_atom_forge_write(&forge, param_data, param_size);

                lv2_atom_forge_pop(&forge, &frame);
            }
            if (param->is_readable) {
                // send request to read parameter
                lv2_atom_forge_frame_time(&forge, 0);
                lv2_atom_forge_object(&forge, &frame, 0, uri_map(LV2_PATCH__Get));

                lv2_atom_forge_key(&forge, uri_map(LV2_PATCH__subject));
                lv2_atom_forge_urid(&forge, uri_map(plugin_uri.c_str()));

                lv2_atom_forge_key(&forge, uri_map(LV2_PATCH__property));
                lv2_atom_forge_urid(&forge, param->id);

                lv2_atom_forge_pop(&forge, &frame);
            }
        }

        patch_in->header.atom.size += lv2_atom_pad_size(forge.offset);
    }

    // send time and tempo information to plugin
    if (time_in) {
        assert(song);

        float song_tempo = song->tempo;
        bool song_playing = song->is_playing;

        if (song_last_tempo != song_tempo || song_last_playing != song_playing) {
            song_last_tempo = song_tempo;
            song_last_playing = song_playing;

            LV2_Atom_Event* event = lv2_atom_sequence_end(&time_in->header.body, time_in->header.atom.size);
            lv2_atom_forge_set_buffer(
                &forge,
                (uint8_t*) event,
                ATOM_SEQUENCE_CAPACITY - ((size_t)(event) - (size_t)(time_in->data))
            );

            LV2_Atom_Forge_Frame frame;

            // write time position data
            lv2_atom_forge_frame_time(&forge, 0);
            lv2_atom_forge_object(&forge, &frame, 0, uri_map(LV2_TIME__Position));

            // beat in bars
            lv2_atom_forge_key(&forge, uri_map(LV2_TIME__barBeat));
            lv2_atom_forge_float(&forge, song->position);

            // beats per minute
            lv2_atom_forge_key(&forge, uri_map(LV2_TIME__beatsPerMinute));
            lv2_atom_forge_float(&forge, song_tempo);

            // play/stop
            lv2_atom_forge_key(&forge, uri_map(LV2_TIME__speed));
            lv2_atom_forge_float(&forge, song_playing ? 1.0f : 0.0f);

            lv2_atom_forge_pop(&forge, &frame);

            time_in->header.atom.size += lv2_atom_pad_size(forge.offset);
        }
    }

    lilv_instance_run(instance, sample_count);

    // clear input message streams
    for (AtomSequenceBuffer* buf : msg_in)
        lv2_atom_sequence_clear(&buf->header);

    // write output buffers
    convert_to_stereo(
        &audio_output_bufs.front(),
        output,
        audio_output_bufs.size(),
        _dest.frames_per_buffer,
        interleave
    );
}

void Lv2Plugin::event(const audiomod::MidiEvent* midi_event)
{
    if (midi_in)
    {
        struct {
            LV2_Atom_Event header;
            audiomod::MidiMessage midi;
        } atom;

        const audiomod::MidiMessage& midi_msg = midi_event->msg;

        atom.header.time.frames = 0;
        atom.header.body.size = midi_msg.size();
        atom.header.body.type = uri_map(LV2_MIDI__MidiEvent);
        memcpy(&atom.midi, &midi_msg, midi_msg.size());

        lv2_atom_sequence_append_event(&midi_in->header, ATOM_SEQUENCE_CAPACITY, &atom.header);
    }
}

size_t Lv2Plugin::receive_events(void** handle, audiomod::MidiEvent* buffer, size_t capacity)
{
    if (midi_out)
    {
        LV2_Atom_Event** it = (LV2_Atom_Event**) handle;

        LV2_Atom_Sequence& seq = midi_out->header;

        if (*it == nullptr)
            *it = lv2_atom_sequence_begin(&seq.body);

        size_t count = 0;

        while (true)
        {
            // ran out of buffer space, yield "coroutine"
            if (count >= capacity)
                break;

            // reached end of sequence, "coroutine" is finished
            if (lv2_atom_sequence_is_end(&seq.body, seq.atom.size, *it)) {
                break;
            }

            // if event is a midi event
            LV2_Atom_Event* &ev = *it;
            if (ev->body.type == uri_map(LV2_MIDI__MidiEvent))
            {
                const audiomod::MidiMessage* const midi_ev = (const audiomod::MidiMessage*) (ev + 1);
                
                buffer[count++] = {
                    (uint64_t) ev->time.frames,
                    *midi_ev
                };
            }

            *it = lv2_atom_sequence_next(*it);
        }

        return count;
    }

    return 0;
}

void Lv2Plugin::flush_events()
{
    // read patch:Put and patch:Set events
    if (patch_out) {
        LV2_ATOM_SEQUENCE_FOREACH (&patch_out->header, ev)
        {
            if (ev->body.type == uri_map(LV2_ATOM__Object))
            {
                const LV2_Atom_Object* obj = (const LV2_Atom_Object*)&ev->body;

                // return from patch:Get <property>
                if (obj->body.otype == uri_map(LV2_PATCH__Set)) {
                    const LV2_Atom_URID* property   = nullptr;
                    const LV2_Atom* value           = nullptr;

                    lv2_atom_object_get(obj,
                        uri_map(LV2_PATCH__property),   (const LV2_Atom**)&property,
                        uri_map(LV2_PATCH__value),      &value,  
                    0);

                    assert(property && property->atom.type == uri_map(LV2_ATOM__URID) && value);
                    if (property && property->atom.type == uri_map(LV2_ATOM__URID) && value) {
                        Parameter* param = find_parameter(property->body);
                        assert(param);

                        if (param) {
                            param->set(value + 1, value->size, value->type);
                        }
                    }
                }
            }
        }
    }

    // clear event sequences
    for (AtomSequenceBuffer* buf : msg_out) {
        lv2_atom_sequence_clear(&buf->header);
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