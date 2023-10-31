#include <iostream>
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
#include <cmath>

#include "lv2interface.h"
#include "lv2internal.h"
#include "../../util.h"
#include "../../song.h"

using namespace plugins;
using namespace lv2;

Lv2PluginHost::Lv2PluginHost(audiomod::ModuleContext& modctx, const PluginData& plugin_data, WorkScheduler& scheduler)
    : plugin_data(plugin_data),
      worker_host(scheduler),
      _modctx(modctx)
{
    if (plugin_data.type != PluginType::Lv2)
        throw std::runtime_error("mismatched plugin types");

    // get plugin uri
    Lilv_ptr<LilvNode> plugin_uri; {
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

    lv2_plugin_data = plugin;

    // init features
    map = {nullptr, uri::map_callback};
    map_feature = {LV2_URID__map, &map};
    unmap = {nullptr, uri::unmap_callback};
    unmap_feature = {LV2_URID__unmap, &unmap};
    log = {nullptr, log::printf, log::vprintf};
    log_feature = {LV2_LOG__log, &log};
    lv2_worker_schedule = {&worker_host, WorkerHost::_schedule_work};
    work_schedule_feature = {LV2_WORKER__schedule, &lv2_worker_schedule};
    options[0] = {
        LV2_OPTIONS_INSTANCE, 0, 0, 0, 0, 0
    };
    options_feature = {LV2_OPTIONS__options, &options};

    // instantiate plugin
    instance = lilv_plugin_instantiate(plugin, _modctx.sample_rate, features);
    if (instance == nullptr) {
        dbg("ERROR: plugin instantiation failed!\n");
        throw lv2_error("instantiation failed");
    }

    // init worker feature
    worker_host.worker_interface = (LV2_Worker_Interface*) lilv_instance_get_extension_data(instance, LV2_WORKER__interface);
    worker_host.instance = lilv_instance_get_handle(instance);

    // create and connect ports
    const uint32_t port_count = lilv_plugin_get_num_ports(plugin);

    // get port ranges
    std::vector<float> min_values, max_values, default_values;
    min_values.reserve(port_count);
    max_values.reserve(port_count);
    default_values.reserve(port_count);
    
    bool unsupported = false;
    lilv_plugin_get_port_ranges_float(
        plugin,
        min_values.data(),
        max_values.data(),
        default_values.data()
    );

    for (uint32_t i = 0; i < port_count; i++)
    {
        const LilvPort* port = lilv_plugin_get_port_by_index(plugin, i);
        
        Lilv_ptr name_node = lilv_port_get_name(plugin, port);
        const LilvNode* symbol_n = lilv_port_get_symbol(plugin, port);
        const char* port_name = name_node ? lilv_node_as_string(name_node) : "[unnamed]";
        const char* port_symbol = lilv_node_as_string(symbol_n);

        bool is_input_port = lilv_port_is_a(plugin, port, URI.lv2_InputPort);
        bool is_output_port = lilv_port_is_a(plugin, port, URI.lv2_OutputPort);
        bool is_optional = lilv_port_has_property(plugin, port, URI.lv2_connectionOptional);

        Lilv_ptr designation_n = lilv_port_get(plugin, port, URI.lv2_designation);

        /*
        TODO
        lilv_port_get_value(plugin, port, URI.lv2_portProperty)
        pprops:logarithmic
        */

        // INPUT CONTROL PORT
        if (
            lilv_port_is_a(plugin, port, URI.lv2_ControlPort) &&
            is_input_port
        ) {
            ControlInputPort* ctl = new ControlInputPort;
            ctl->name = port_name;
            ctl->symbol = port_symbol;
            ctl->port_index = i;
            ctl->port_handle = port;
            ctl->min = min_values[i];
            ctl->max = max_values[i];
            ctl->default_value = default_values[i];
            ctl->value = ctl->default_value;
            ctl->unit = UnitType::None;

            Lilv_ptr unit_n = lilv_port_get(plugin, port, URI.units_unit);
            
            if (unit_n) {
                // TODO: more units
                if (lilv_node_equals(unit_n, URI.units_db))
                    ctl->unit = UnitType::dB;
            }

            // connect port
            lilv_instance_connect_port(instance, i, &ctl->value);

            // save port data
            PortData port_data;
            port_data.is_output = false;
            port_data.type = PortData::Control;
            port_data.symbol = ctl->symbol.c_str();
            port_data.ctl_in = ctl;

            ctl_in.push_back(ctl);
            ports[i] = port_data;

            // add to interface
            InterfaceDisplay display;
            display.is_parameter = false;
            display.read_only = false;
            display.port_in = ctl;
            input_displays.push_back(display);
        }

        // OUTPUT CONTROL PORT
        else if (
            lilv_port_is_a(plugin, port, URI.lv2_ControlPort) &&
            is_output_port
        ) {
            ControlOutputPort* ctl = new ControlOutputPort;
            ctl->name = port_name;
            ctl->port_index = i;
            ctl->symbol = port_symbol;
            ctl->port_handle = port;

            lilv_instance_connect_port(instance, i, &ctl->value);

            // save port data
            PortData port_data;
            port_data.is_output = true;
            port_data.type = PortData::Control;
            port_data.symbol = ctl->symbol.c_str();
            port_data.ctl_out = ctl;

            ctl_out.push_back(ctl);
            ports[i] = port_data;
            
            // add to interface
            InterfaceDisplay display;
            display.is_parameter = false;
            display.read_only = true;
            display.port_out = ctl;
            output_displays.push_back(display);
        }

        // INPUT AUDIO PORT
        else if (
            lilv_port_is_a(plugin, port, URI.lv2_AudioPort) &&
            is_input_port
        ) {
            float* input_buf = new float[_modctx.frames_per_buffer * 2];
            audio_input_bufs.push_back(input_buf);
            lilv_instance_connect_port(instance, i, input_buf);
        }

        // OUTPUT AUDIO PORT
        else if (
            lilv_port_is_a(plugin, port, URI.lv2_AudioPort) &&
            is_output_port
        ) {
            float* output_buf = new float[_modctx.frames_per_buffer * 2];
            audio_output_bufs.push_back(output_buf);
            lilv_instance_connect_port(instance, i, output_buf);
        }

        // ATOM PORT
        else if (lilv_port_is_a(plugin, port, URI.atom_AtomPort))
        {
            if (is_input_port || is_output_port) 
            {
                Lilv_ptr buffer_type_n = lilv_port_get(plugin, port, URI.atom_bufferType);
                LilvNodes* supports_n = lilv_port_get_value(plugin, port, URI.atom_supports);

                bool is_main = lilv_node_equals(designation_n, URI.lv2_control);
                bool midi_support = lilv_nodes_contains(supports_n, URI.midi_MidiEvent);
                bool patch_support = lilv_nodes_contains(supports_n, URI.patch_Message);
                bool time_support = lilv_nodes_contains(supports_n, URI.time_Position);

                // atom:Sequence
                if (lilv_node_equals(buffer_type_n, URI.atom_Sequence)) {
                    AtomSequencePort* seq_buf = new AtomSequencePort {
                        port, port_symbol,

                        {
                            { sizeof(LV2_Atom_Sequence_Body), uri::map(LV2_ATOM__Sequence) },
                            { 0, 0 }
                        },

                        MessageQueue(1, ATOM_SEQUENCE_CAPACITY)
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

                    lilv_instance_connect_port(instance, i, &seq_buf->data.header);

                    PortData port_data;
                    port_data.is_output = is_output_port;
                    port_data.type = PortData::AtomSequence;
                    port_data.sequence = seq_buf;
                    port_data.symbol = seq_buf->symbol.c_str();
                    ports[i] = port_data;
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
    Lilv_ptr patch_writable_n = lilv_world_find_nodes(
        LILV_WORLD,
        plugin_uri,
        URI.patch_writable,
        NULL
    );

    LILV_FOREACH (nodes, iter, patch_writable_n) {
        const LilvNode* property = lilv_nodes_get(patch_writable_n, iter);
        
        Lilv_ptr label_n = lilv_world_get(LILV_WORLD, property, URI.rdfs_label, NULL);
        Lilv_ptr type_n = lilv_world_get(LILV_WORLD, property, URI.rdfs_range, NULL);

        if (label_n && type_n) {
            Parameter* param = new Parameter(
                lilv_node_as_uri(property),
                lilv_node_as_string(label_n),
                lilv_node_as_uri(type_n)
            );

            param->is_writable = true;
            param->is_readable = false;
            parameters.push_back(param);
        } else {
            dbg("WARNING: parameter %s has no label or type", lilv_node_as_string(property));
        }
    }

    // scan readable parameters
    Lilv_ptr patch_readable_n = lilv_world_find_nodes(
        LILV_WORLD,
        plugin_uri,
        URI.patch_readable,
        NULL
    );

    LILV_FOREACH (nodes, iter, patch_readable_n) {
        const LilvNode* property = lilv_nodes_get(patch_readable_n, iter);
        
        Lilv_ptr label_n = lilv_world_get(LILV_WORLD, property, URI.rdfs_label, NULL);
        Lilv_ptr type_n = lilv_world_get(LILV_WORLD, property, URI.rdfs_range, NULL);

        if (label_n && type_n) {
            Parameter* param = find_parameter( uri::map(lilv_node_as_uri(property)) );
            if (param == nullptr) {
                param = new Parameter(
                    lilv_node_as_uri(property),
                    lilv_node_as_string(label_n),
                    lilv_node_as_uri(type_n)
                );
                parameters.push_back(param);
            }

            param->is_readable = true;
        } else {
            dbg("WARNING: parameter %s has no label or type", lilv_node_as_string(property));
        }
    }

    // load default state
    if (lilv_plugin_has_feature(plugin, URI.state_loadDefaultState)) {
        Lilv_ptr state = lilv_state_new_from_world(LILV_WORLD, &map, plugin_uri);
        
        if (state) {
            lilv_state_restore(state, instance, set_port_value_callback, this, 0, features);
        }
    }

    // add parameters to interface
    for (Parameter* param : parameters) {
        // TODO: string/path parameters
        if (param->type != Parameter::TypeString && param->type != Parameter::TypePath) {
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

    input_combined = new float[_modctx.frames_per_buffer * 2];

    // create utility atom forge
    lv2_atom_forge_init(&forge, &map);
}

void Lv2PluginHost::destroy()
{
    dbg("free LV2 plugin host\n");
    stop();
    lilv_instance_free(instance);

}
#define FREE_ARRAY(array, operator) \
    for (auto val : array) \
        operator val;

Lv2PluginHost::~Lv2PluginHost()
{
    // TODO: stop using raw pointers

    delete[] input_combined;
    FREE_ARRAY(audio_input_bufs, delete[]);
    FREE_ARRAY(audio_output_bufs, delete[]);
    FREE_ARRAY(ctl_in, delete);
    FREE_ARRAY(ctl_out, delete);
    FREE_ARRAY(msg_in, delete);
    FREE_ARRAY(msg_out, delete);
    FREE_ARRAY(parameters, delete);
}

void Lv2PluginHost::start()
{
    lilv_instance_activate(instance);
}

void Lv2PluginHost::stop()
{
    lilv_instance_deactivate(instance);
}

// TODO: i feel like this function is too long
// it could be broken up into seperate functions, e.g.
// update_events, write_events
void Lv2PluginHost::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count)
{
    bool interleave = false;

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
        _modctx.frames_per_buffer,
        interleave
    );

    // send buffer capacity to plugins
    for (AtomSequencePort* out : msg_out)
        out->data.header.atom.size = ATOM_SEQUENCE_CAPACITY;

    // copy received events
    for (AtomSequencePort* in : msg_in)
    {
        while (true)
        {
            std::byte ev_buf[256];

            auto handle = in->shared_queue.read();
            if (handle.size() == 0) break;
            assert(handle.size() < sizeof(ev_buf));

            handle.read(ev_buf, handle.size());

            lv2_atom_sequence_append_event(&in->data.header, ATOM_SEQUENCE_CAPACITY, (LV2_Atom_Event*) ev_buf);
        }
        //auto handle = in->shared.get_handle();
        //in->data = handle.get();
        //lv2_atom_sequence_clear(&handle.get().header);
    }

    // set and monitor required parameters
    if (patch_in)
    {
        LV2_Atom_Event* event = lv2_atom_sequence_end(&patch_in->data.header.body, patch_in->data.header.atom.size);
        lv2_atom_forge_set_buffer(
            &forge,
            (uint8_t*) event,
            ATOM_SEQUENCE_CAPACITY - ((size_t)(event) - (size_t)(patch_in->data.data))
        );

        LV2_Atom_Forge_Frame frame;

        for (Parameter* param : parameters)
        {
            if (param->is_writable && param->did_change) {
                param->did_change = false;
                
                // send request to write parameter
                dbg("%s changed\n", uri::unmap(param->id));
                
                lv2_atom_forge_frame_time(&forge, 0);
                lv2_atom_forge_object(&forge, &frame, 0, uri::map(LV2_PATCH__Set));

                lv2_atom_forge_key(&forge, uri::map(LV2_PATCH__subject));
                lv2_atom_forge_urid(&forge, uri::map(plugin_uri.c_str()));

                lv2_atom_forge_key(&forge, uri::map(LV2_PATCH__property));
                lv2_atom_forge_urid(&forge, param->id);

                uint32_t param_size;
                const void* param_data = param->get(&param_size);

                lv2_atom_forge_key(&forge, uri::map(LV2_PATCH__value));
                lv2_atom_forge_atom(&forge, param_size, uri::map(param->type_uri()));
                lv2_atom_forge_write(&forge, param_data, param_size);

                lv2_atom_forge_pop(&forge, &frame);
            }
            if (param->is_readable) {
                // send request to read parameter
                lv2_atom_forge_frame_time(&forge, 0);
                lv2_atom_forge_object(&forge, &frame, 0, uri::map(LV2_PATCH__Get));

                lv2_atom_forge_key(&forge, uri::map(LV2_PATCH__subject));
                lv2_atom_forge_urid(&forge, uri::map(plugin_uri.c_str()));

                lv2_atom_forge_key(&forge, uri::map(LV2_PATCH__property));
                lv2_atom_forge_urid(&forge, param->id);

                lv2_atom_forge_pop(&forge, &frame);
            }
        }

        patch_in->data.header.atom.size += lv2_atom_pad_size(forge.offset);
    }

    // send time and tempo information to plugin
    if (time_in) {
        assert(song);

        float song_tempo = song->tempo;
        bool song_playing = song->is_playing;

        if (song_last_tempo != song_tempo || song_last_playing != song_playing) {
            song_last_tempo = song_tempo;
            song_last_playing = song_playing;

            LV2_Atom_Event* event = lv2_atom_sequence_end(&time_in->data.header.body, time_in->data.header.atom.size);
            lv2_atom_forge_set_buffer(
                &forge,
                (uint8_t*) event,
                ATOM_SEQUENCE_CAPACITY - ((size_t)(event) - (size_t)(time_in->data.data))
            );

            LV2_Atom_Forge_Frame frame;

            // write time position data
            lv2_atom_forge_frame_time(&forge, 0);
            lv2_atom_forge_object(&forge, &frame, 0, uri::map(LV2_TIME__Position));

            // beat in bars
            lv2_atom_forge_key(&forge, uri::map(LV2_TIME__barBeat));
            lv2_atom_forge_float(&forge, song->position);

            // beats per minute
            lv2_atom_forge_key(&forge, uri::map(LV2_TIME__beatsPerMinute));
            lv2_atom_forge_float(&forge, song_tempo);

            // play/stop
            lv2_atom_forge_key(&forge, uri::map(LV2_TIME__speed));
            lv2_atom_forge_float(&forge, song_playing ? 1.0f : 0.0f);

            lv2_atom_forge_pop(&forge, &frame);

            time_in->data.header.atom.size += lv2_atom_pad_size(forge.offset);
        }
    }

    lilv_instance_run(instance, sample_count);
    
    worker_host.process_responses();
    worker_host.end_run();

    // clear input message streams
    for (AtomSequencePort* in : msg_in)
        lv2_atom_sequence_clear(&in->data.header);

    // write output buffers
    convert_to_stereo(
        &audio_output_bufs.front(),
        output,
        audio_output_bufs.size(),
        _modctx.frames_per_buffer,
        interleave
    );

    // port notifications
    // copy port values to the UIHost
    _writing_notifs = true;

    for (auto& [ index, queue ] : port_notification_targets)
    {
        const PortData& port_data = ports[index];

        switch (port_data.type) {
            case PortData::Control: {
                float value = port_data.is_output ? port_data.ctl_out->value : port_data.ctl_in->value;
                queue->post(&value, sizeof(float));
                break;
            }

            case PortData::AtomSequence: {
                AtomSequencePort* in = port_data.sequence;

                LV2_ATOM_SEQUENCE_FOREACH(&in->data.header, ev)
                {
                    queue->post(ev, sizeof(*ev) + ev->body.size);
                }

                /*auto write_handle = out->get_handle();
                AtomSequenceBuffer& buf = write_handle.get();

                LV2_ATOM_SEQUENCE_FOREACH (&in->data.header, ev)
                    lv2_atom_sequence_append_event(&buf.header, ATOM_SEQUENCE_CAPACITY, ev);*/
                
                break;

                // write handle will unlock when destroyed
            }
        }
    }

    _writing_notifs = false;

    // read patch:Put and patch:Set events
    if (patch_out) {
        LV2_ATOM_SEQUENCE_FOREACH (&patch_out->data.header, ev)
        {
            if (ev->body.type == uri::map(LV2_ATOM__Object))
            {
                const LV2_Atom_Object* obj = (const LV2_Atom_Object*)&ev->body;

                // return from patch:Get <property>
                if (obj->body.otype == uri::map(LV2_PATCH__Set)) {
                    const LV2_Atom_URID* property   = nullptr;
                    const LV2_Atom* value           = nullptr;

                    lv2_atom_object_get(obj,
                        uri::map(LV2_PATCH__property),   (const LV2_Atom**)&property,
                        uri::map(LV2_PATCH__value),      &value,  
                    0);

                    assert(property && property->atom.type == uri::map(LV2_ATOM__URID) && value);
                    if (property && property->atom.type == uri::map(LV2_ATOM__URID) && value) {
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
}

void Lv2PluginHost::event(const audiomod::MidiEvent& midi_event)
{
    if (midi_in)
    {
        struct {
            LV2_Atom_Event header;
            audiomod::MidiMessage midi;
        } atom;

        const audiomod::MidiMessage& midi_msg = midi_event.msg;

        atom.header.time.frames = 0;
        atom.header.body.size = midi_msg.size();
        atom.header.body.type = uri::map(LV2_MIDI__MidiEvent);
        memcpy(&atom.midi, &midi_msg, midi_msg.size());

        //auto handle = midi_in->shared.get_handle();
        //lv2_atom_sequence_append_event(&handle.get().header, ATOM_SEQUENCE_CAPACITY, &atom.header);

        lv2_atom_sequence_append_event(&midi_in->data.header, ATOM_SEQUENCE_CAPACITY, &atom.header);
    }
}

void Lv2PluginHost::send_events(audiomod::ModuleBase& target)
{
    if (midi_out)
    {
        LV2_ATOM_SEQUENCE_FOREACH(&midi_out->data.header, ev)
        {
            // if event is a midi event
            if (ev->body.type == uri::map(LV2_MIDI__MidiEvent))
            {
                // get midi message written after the end of the event
                const audiomod::MidiMessage* const midi_ev = (const audiomod::MidiMessage*) (ev + 1);

                // convert it to a NoteEvent and send it to the target module
                audiomod::NoteEvent note_ev;
                note_ev.read_midi(midi_ev);
                target.event(note_ev);
            }
        }
    }
}

void Lv2PluginHost::port_subscribe(uint32_t port_index, std::shared_ptr<MessageQueue>& queue)
{
    auto it = ports.find(port_index);
    if (it == ports.end()) return;
    auto& [ index, port_data ] = *it;
    assert(index == port_index);

    port_notification_targets[index] = queue;
}

void Lv2PluginHost::port_unsubscribe(uint32_t port_index)
{
    port_notification_targets.erase(port_index);
}

// TODO
void Lv2PluginHost::save_state(std::ostream& ostream)
{
    return;
}

// TODO
bool Lv2PluginHost::load_state(std::istream& istream, size_t size)
{
    return false;
}