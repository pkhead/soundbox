#include <cassert>
#include <cstring>
#include <string>
#include <iostream>
#include <cstdio>

#include <lv2/units/units.h>
#include <lv2/urid/urid.h>
#include <lv2/log/log.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>
#include <lv2/atom/util.h>
#include <lv2/midi/midi.h>
#include <lv2/time/time.h>
#include <lv2/patch/patch.h>
#include <lv2/state/state.h>
#include <lv2/ui/ui.h>
#include <suil/suil.h>
#include "lv2interface.h"
#include "lv2internal.h"
#include "../../util.h"
#include "../../song.h"

using namespace plugins;
using namespace lv2;

LilvWorld* lv2::LILV_WORLD;
LilvNodeUriList lv2::URI;

void Lv2Plugin::lv2_init(int* argc, char*** argv) {
    LILV_WORLD = lilv_world_new();

    // TODO: pass in arguments from main
    suil_init(argc, argv, SUIL_ARG_NONE);

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

    URI.ui_X11UI = lilv_new_uri(LILV_WORLD, LV2_UI__X11UI);
    URI.ui_WindowsUI = lilv_new_uri(LILV_WORLD, LV2_UI__WindowsUI);
    URI.ui_parent = lilv_new_uri(LILV_WORLD, LV2_UI__parent);

    URI.units_unit = lilv_new_uri(LILV_WORLD, LV2_UNITS__unit);
    URI.units_db = lilv_new_uri(LILV_WORLD, LV2_UNITS__db);
}

void Lv2Plugin::lv2_fini() {
    lilv_world_free(LILV_WORLD);
}

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


















/////////////////////
// LV2 log feature //
/////////////////////

int log::printf(LV2_Log_Handle handle, LV2_URID type, const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    int res = std::vprintf(fmt, va);
    va_end(va);
    return res;
}

int log::vprintf(LV2_Log_Handle handle, LV2_URID type, const char* fmt, va_list ap)
{
    return std::vprintf(fmt, ap);
}


















/////////////////
// URI mapping //
/////////////////

LV2_URID uri::map(const char* uri)
{
    LV2_URID id;
    for (id = 0; id < URI_MAP.size(); id++) {
        if (URI_MAP[id] == uri)
            return id;
    }

    URI_MAP.push_back(uri);
    return id;
}

const char* uri::unmap(LV2_URID urid)
{
    if (urid >= URI_MAP.size())
        return nullptr;

    return URI_MAP[urid].c_str();
}

// support for lv2 urid feature
LV2_URID uri::map_callback(LV2_URID_Map_Handle handle, const char* uri)
{
    return map(uri);
}

const char* uri::unmap_callback(LV2_URID_Map_Handle handle, LV2_URID urid)
{
    return unmap(urid);
}



















////////////////////////
// LV2 worker feature //
////////////////////////

WorkerHost::WorkerHost(WorkScheduler& work_scheduler) :
    work_scheduler(work_scheduler)
{}

LV2_Worker_Status WorkerHost::_schedule_work(LV2_Worker_Schedule_Handle handle, uint32_t size, const void* userdata)
{
    WorkerHost* self = (WorkerHost*) handle;

    if (self->worker_interface == nullptr) {
        DBGBREAK;
        return LV2_WORKER_ERR_UNKNOWN;
    }

    work_data_payload_t data;

    data.self = self;

    if (userdata && size)
        memcpy(&data.userdata, userdata, size);

    if (self->work_scheduler.schedule(_work_proc, &data, size + sizeof(Lv2Plugin*)))
        return LV2_WORKER_SUCCESS;
    else
        return LV2_WORKER_ERR_NO_SPACE;
}

void WorkerHost::_work_proc(void* data, size_t size)
{
    work_data_payload_t* payload = (work_data_payload_t*) data;
    WorkerHost* self = payload->self;

    self->worker_interface->work(
        self->instance,
        _worker_respond,
        self,
        size,
        data
    );
}

LV2_Worker_Status WorkerHost::_worker_respond(
    LV2_Worker_Respond_Handle handle,
    uint32_t size,
    const void* data
) {
    WorkerHost* self = (WorkerHost*) handle;

    if (self->worker_interface == nullptr) {
        DBGBREAK;
        return LV2_WORKER_ERR_UNKNOWN;
    }

    for (int i = 0; i < RESPONSE_QUEUE_CAPACITY; i++)
    {
        WorkerResponse& response = self->worker_responses[i];
        if (response.active) continue;

        response.size = size;
        if (data && response.size) {
            memcpy(response.data, data, size);
            return LV2_WORKER_SUCCESS;
        }

        response.active = true;
    }

    return LV2_WORKER_ERR_NO_SPACE;
}

void WorkerHost::process_responses()
{
    if (worker_interface)
    {
        for (int i = 0; i < RESPONSE_QUEUE_CAPACITY; i++)
        {
            WorkerResponse& response = worker_responses[i];
            if (!response.active) continue;

            worker_interface->work_response(
                instance,
                response.size,
                response.data
            );

            response.active = false;
        }
    }
}

void WorkerHost::end_run()
{
    if (worker_interface && worker_interface->end_run)
        worker_interface->end_run(instance);
}




///////////////////////
// LV2 state feature //
///////////////////////

Lv2Plugin::Parameter::Parameter(const char* urid, const char* label, const char* type_uri)
    : id(uri::map(urid)),
      did_change(false),
      label(label)
{
    if (strcmp(type_uri, LV2_ATOM__Int) == 0) {
        type = TypeInt;
        v._int = 0;
    } else if (strcmp(type_uri, LV2_ATOM__Long) == 0) {
        type = TypeLong;
        v._long = 0;
    } else if (strcmp(type_uri, LV2_ATOM__Float) == 0) {
        type = TypeFloat;
        v._float = 0;
    } else if (strcmp(type_uri, LV2_ATOM__Double) == 0) {
        type = TypeDouble;
        v._double = 0;
    } else if (strcmp(type_uri, LV2_ATOM__Bool) == 0) {
        type = TypeBool;
        v._bool = 0;
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

bool Lv2Plugin::Parameter::set(const void* value, uint32_t expected_size, uint32_t expected_type)
{
    if (uri::map(type_uri()) != expected_type) return false;

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