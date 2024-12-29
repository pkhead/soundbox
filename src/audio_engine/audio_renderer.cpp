#include <cstring>
#include <functional>
#include <algorithm>
#include "audio_renderer.hpp"
#include "audio_engine.hpp"
#include "../log.hpp"
#include "audio_engine/module_data.hpp"
#include "dsp.hpp"

using namespace modules;

AudioRenderer::AudioRenderer(AudioEngine &engine) :
    engine(engine),
    in_queue(128),
    out_queue(128)
{
    output_buffer_sz = engine._frames_per_buffer * engine._output_channels;
    output_buffer = nullptr;
    cur_graph = nullptr;
    modulator_sources = new ModulatorSourceList;

    process_time = 0;
    frame_time = 0;
}

AudioRenderer::~AudioRenderer() {
    delete cur_graph;
    delete modulator_sources;
}

void AudioRenderer::_process_node(ModuleID id)
{
    auto &node = cur_graph->nodes[id];

    // call processor
    ModuleProcessor processor(engine._frames_per_buffer, engine._frame_time, engine._sample_rate, cur_graph, id);
    assert(node.module->processor != nullptr);
    node.module->processor(processor);
}

void AudioRenderer::process_audio_out_node(ModuleProcessor& process)
{
    AudioRenderer* self = (AudioRenderer*) process.userdata;
    self->_process_audio_out_node(process);
}

void AudioRenderer::_process_audio_out_node(ModuleProcessor& process)
{
    float* input_buffer = process.audio_input(0);
    assert(process.audio_input_channels(0) == engine._output_channels);

    assert(output_buffer_sz % engine._output_channels == 0);
    assert(output_buffer_sz == process.buffer_frame_count * process.audio_input_channels(0));

    // copy input of the AUDIO_OUT module to the output audio ring buffer
    assert(output_buffer != nullptr);
    memcpy(output_buffer, input_buffer, process.buffer_frame_count * engine._output_channels * sizeof(float));
}

void AudioRenderer::process_stereo_mixer_node(ModuleProcessor &proc)
{
    assert(proc.audio_output_channels(0) == 2);
    float *out = proc.audio_output(0);
    memset(out, 0, proc.buffer_frame_count * 2 * sizeof(float));

    unsigned int input_index = 0;
    while (true)
    {
        float *in = proc.audio_input(input_index++);
        if (in == nullptr) break;

        for (unsigned int i = 0; i < proc.buffer_frame_count * 2; i++)
        {
            out[i] += *in++;
        }
    }
}

void AudioRenderer::process_message_duplicator_node(ModuleProcessor &proc)
{
    static std::byte buf[AudioEngine::MESSAGE_PORT_CAPACITY];

    while (true) {
        unsigned int msg_size = proc.read_message(0, (void*) buf, AudioEngine::MESSAGE_PORT_CAPACITY);
        if (msg_size == 0) break;

        for (unsigned int i = 0; i < proc.node.message_outputs.size(); i++) {
            proc.send_message(i, (void*) buf, msg_size);
        }
    }
}

ModuleData::ModulatorInstance ModuleData::ModulatorSource::instantiate() const {
    ModulatorInstance inst;
    inst.src = this;
    inst.cur_level = 0.0f;
    return inst;
}

void ModuleData::ModulatorSource::get_params(ModulatorSourceParams &params) const {
    params.attack = env_params.attack;
    params.decay = env_params.decay;
    params.sustain = env_params.sustain;
    params.release = env_params.release;
}

void ModuleData::ModulatorSource::apply_params(const ModulatorSourceParams &params) {
    env_params.attack = params.attack;
    env_params.decay = params.decay;
    env_params.sustain = params.sustain;
    env_params.release = params.release;
}

void ModuleData::ModulatorSource::next_envelope_sample(ModulatorInstance &inst) const {
    inst.envelope.compute(sample_rate, inst.cur_level, env_params);
}



float ModuleData::EnvelopeModulatorSource::next_sample(ModulatorInstance &inst) const {
    next_envelope_sample(inst);
    return inst.cur_level;
}



ModuleData::ModulatorInstance ModuleData::LFOModulatorSource::instantiate() const {
    auto params = ModulatorSource::instantiate();
    params.phase = 0.0f;
    return params;
}

void ModuleData::LFOModulatorSource::get_params(ModulatorSourceParams &params) const {
    ModulatorSource::get_params(params);
    params.lfo.wavetype = wavetype;
    params.lfo.amp = amp;
    params.lfo.freq = freq;
}

void ModuleData::LFOModulatorSource::apply_params(const ModulatorSourceParams &params) {
    ModulatorSource::apply_params(params);
    wavetype = params.lfo.wavetype;
    amp = params.lfo.amp;
    freq = params.lfo.freq;
}

float ModuleData::LFOModulatorSource::next_sample(ModulatorInstance &inst) const {
    next_envelope_sample(inst);
    assert(false);
    return 0.0f;
}

// void AudioRenderer::update_modulator_target(ModuleID mod_id, unsigned int moduidx, const ModuleData::ModulatorTarget &params) {
//     if (cur_graph == nullptr) return;

//     auto &node = cur_graph->nodes[mod_id];
//     assert(moduidx < node.modulators.size());
    
//     for (auto &target : node.modulators[moduidx].targets) {
//         if (target.control_index == params.control_index) {
//             target = params;
//             return;
//         }
//     }

//     logger::log_warning("AudioRenderer::update_modulator_target: could not find target for control %i", params.control_index);
// }

void AudioRenderer::render(float *buf)
{
    output_buffer = buf;
    
    // read input queue
    InMessage in_msg;
    while (in_queue.try_dequeue(in_msg)) {
        switch (in_msg.kind) {
            case InMessageKind::MESSAGE_NEW_GRAPH: {
                discard_object(cur_graph);
                cur_graph = in_msg.graph;
                
                OutMessage out_msg;
                out_msg.kind = OutMessageKind::MESSAGE_GRAPH_UPDATED;
                out_queue.try_enqueue(out_msg);

                break;
            }

            case InMessageKind::MESSAGE_UPDATE_MODULATOR_SOURCE_LIST: {
                discard_object(modulator_sources);
                modulator_sources = in_msg.modulator_source_list;
                break;
            }

            case InMessageKind::MESSAGE_UPDATE_MODULATOR_SOURCE_PARAMS: {
                auto &payload = in_msg.modulator_source_params;

                ModulatorSourceID id = payload.src_id;
                const auto &it = modulator_sources->find(id);
                assert(it != modulator_sources->end());
                if (it == modulator_sources->end()) break;
                //if (it == modulator_sources->end()) {
                //    logger::log_error("failed to update modulator source params: mod id %i was not synced", id);
                //    break;
                //}

                it->second->apply_params(payload.params);
                break; // fuck it took me two hours to notice I forgot this
            }

            case InMessageKind::MESSAGE_UPDATE_MODULE_MODULATORS: {
                auto &payload = in_msg.module_modulators;

                assert(cur_graph != nullptr);
                const auto mod_it = cur_graph->nodes.find(payload.mod_id);
                assert(mod_it != cur_graph->nodes.end());
                if (mod_it == cur_graph->nodes.end()) break;
                //if (mod_it == cur_graph->nodes.end()) {
                //    logger::log_error("failed to update control modulator params. mod id %i was not synced?", payload.mod_id);
                //    break;
                //}

                auto &node = mod_it->second;
                discard_object(node.control_modulators);
                node.control_modulators = payload.modulators;

                break;
            }
        }
    }

    // not null if there is an AUDIO_OUT module in the graph
    if (cur_graph != nullptr)
    {
        for (auto &id : cur_graph->process_order) {
            _process_node(id);
        }
    }

    // there are no AUDIO_OUT modules in the graph... just upload a dummy array (full of 0s)
    else
    {
        memset(output_buffer, 0, output_buffer_sz * sizeof(float));
    }
    
    frame_time += engine._frames_per_buffer;
}

AudioRenderer::ModuleGraph* AudioRenderer::build_graph(AudioEngine &engine) {
    std::function<void(ModuleID, int)> calc_depths;

    struct ModuleInfo {
        ModuleID id;
        int depth;
        std::vector<ModuleID> dependencies;
        std::vector<ModuleID> dependents;

        std::vector<GraphConnection> audio_inputs;
        std::vector<GraphConnection> audio_outputs;
        std::vector<GraphConnection> message_inputs;
        std::vector<GraphConnection> message_outputs;

    };

    std::unordered_map<ModuleID, ModuleInfo> module_info;

    // build the entire audio graph starting from the inputs for the AUDIO_OUT module
    // thus, modules that do not contribute to the AUDIO_OUT module do not get processed
    // TODO: should i make stray modules be updated anyway?
    std::function<void(ModuleID, int)> build_graph;
    build_graph = [&](ModuleID id, int depth) {
        std::vector<ModuleID> dependencies;
        std::vector<ModuleID> dependents;
        std::vector<GraphConnection> audio_outputs;
        std::vector<GraphConnection> message_outputs;
        std::vector<GraphConnection> audio_inputs;
        std::vector<GraphConnection> message_inputs;

        std::shared_ptr<ModuleData::ModuleInstance>& inst = engine._modules.at(id);

        // parse dependencies
        unsigned int input_port = 0;
        for (auto it = inst->input_audio_ports.begin(); it != inst->input_audio_ports.end(); it++) {
            if (engine.module_exists(it->connected_module)) {
                // find dependency index of module, adding it to the list
                // if it doesn't already exist
                std::vector<ModuleID>::iterator dep_it = std::find(dependencies.begin(), dependencies.end(), it->connected_module);                
                if (dep_it == dependencies.end()) {
                    dependencies.push_back(it->connected_module);
                    dep_it = dependencies.end() - 1;
                }

                audio_inputs.push_back(GraphConnection {
                    static_cast<int>(dep_it - dependencies.begin()),
                    it->connection_port,
                    input_port
                });
            } else {
                audio_inputs.push_back(GraphConnection {
                    -1, 0, 0
                });
            }

            input_port++;
        }

        input_port = 0;
        for (auto it = inst->input_message_ports.begin(); it != inst->input_message_ports.end(); it++) {
            if (engine.module_exists(it->connected_module)) {
                // find dependency index of module, adding it to the list
                // if it doesn't already exist
                std::vector<ModuleID>::iterator dep_it = std::find(dependencies.begin(), dependencies.end(), it->connected_module);                
                if (dep_it == dependencies.end()) {
                    dependencies.push_back(it->connected_module);
                    dep_it = dependencies.end() - 1;
                }

                audio_inputs.push_back(GraphConnection {
                    static_cast<int>(dep_it - dependencies.begin()),
                    it->connection_port,
                    input_port
                });
            } else {
                audio_inputs.push_back(GraphConnection {
                    -1, 0, 0
                });
            }

            input_port++;
        }

        // parse dependents
        unsigned int output_port = 0;
        for (auto it = inst->output_audio_ports.begin(); it != inst->output_audio_ports.end(); it++) {
            if (engine.module_exists(it->connected_module)) {
                // find dependency index of module, adding it to the list
                // if it doesn't already exist
                std::vector<ModuleID>::iterator dep_it = std::find(dependents.begin(), dependents.end(), it->connected_module);                
                if (dep_it == dependents.end()) {
                    dependents.push_back(it->connected_module);
                    dep_it = dependents.end() - 1;
                }

                audio_outputs.push_back(GraphConnection {
                    static_cast<int>(dep_it - dependents.begin()),
                    output_port,
                    it->connection_port
                });
            } else {
                audio_outputs.push_back(GraphConnection {
                    -1, 0, 0
                });
            }

            output_port++;
        }

        output_port = 0;
        for (auto it = inst->output_message_ports.begin(); it != inst->output_message_ports.end(); it++) {
            if (engine.module_exists(it->connected_module)) {
                // find dependency index of module, adding it to the list
                // if it doesn't already exist
                std::vector<ModuleID>::iterator dep_it = std::find(dependents.begin(), dependents.end(), it->connected_module);                
                if (dep_it == dependents.end()) {
                    dependents.push_back(it->connected_module);
                    dep_it = dependents.end() - 1;
                }

                message_outputs.push_back(GraphConnection {
                    static_cast<int>(dep_it - dependents.begin()),
                    output_port,
                    it->connection_port
                });
            } else {
                message_outputs.push_back(GraphConnection {
                    -1, 0, 0
                });
            }

            output_port++;
        }

        // create module info structure
        module_info[id] = ModuleInfo {
            id,
            depth,
            std::move(dependencies),
            std::move(dependents),
            std::move(audio_inputs),
            std::move(audio_outputs),
            std::move(message_inputs),
            std::move(message_outputs),
        };
        const ModuleInfo &info = module_info[id];

        // recurse, also taking into account module depths to make sure
        // modules are updated in the correct order
        for (auto &dep_id : info.dependencies) {
            auto it = module_info.find(dep_id);
            if (it == module_info.end() || it->second.depth < info.depth) {
                build_graph(dep_id, depth + 1);
            }
        }
    };
    
    ModuleGraph* new_graph = nullptr;

    // find the AUDIO_OUT class to call build_graph
    for (auto &[ id, inst ] : engine._modules)
    {
        if (inst->class_name == AudioEngine::MODULE_CLASS_AUDIO_OUT)
        {
            build_graph(id, 0);

            new_graph = new ModuleGraph;
            auto &proc_order = new_graph->process_order;

            // determine process order from depth values
            for (auto const &[ id, info ] : module_info) {
                proc_order.push_back(id);
            }

            std::sort(proc_order.begin(), proc_order.end(), [&module_info](const ModuleID &a, const ModuleID &b) {
                return module_info[b].depth < module_info[a].depth;
            });

            // create ModuleGraphNodes
            for (auto &[ id, info ] : module_info) {
                new_graph->nodes[id] = ModuleGraphNode {
                    engine._modules.at(id),
                    std::move(info.dependencies),
                    std::move(info.dependents),
                    std::move(info.audio_inputs),
                    std::move(info.audio_outputs),
                    std::move(info.message_inputs),
                    std::move(info.message_outputs),
                    build_modulator_data(engine, id)
                };
            }

            break;
        }
    }

    logger::log_info("node count: %i", new_graph->process_order.size());
    return new_graph;
}

std::vector<AudioRenderer::GraphModulator>* AudioRenderer::build_modulator_data(AudioEngine &engine, ModuleID mod_id) {
    std::vector<AudioRenderer::GraphModulator> list;

    const auto &mod_it = engine._modules.find(mod_id);
    assert(mod_it != engine._modules.end());
    if (mod_it == engine._modules.end()) return nullptr;
    auto &mod = mod_it->second;

    for (auto it = mod->controls.begin(); it != mod->controls.end(); it++) {
        AudioRenderer::GraphModulator data;
        data.operation = it->modop;
        list.push_back(data);
    }

    for (auto &modu : mod->modulators) {
        for (auto target : modu.targets) {
            list[target].sources.push_back(modu.source);
        }
    }

    return new std::vector<AudioRenderer::GraphModulator>(std::move(list));
}