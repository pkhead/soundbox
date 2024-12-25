#include "audio_engine.hpp"
#include "../log.hpp"
#include <cassert>

using namespace modules;

ModuleInfo::ModuleInfo(const std::string &class_name, const std::string &name, bool has_audio_input) :
    class_name(class_name),
    name(name),
    has_audio_input(has_audio_input)
{}








///////////////////////////
// MODULE CREATOR HANDLE //
///////////////////////////

ModuleCreator::ModuleCreator(ModuleID id, AudioEngine& engine, std::string class_name, AudioEngine::ModuleInstance& instance) :
    instance(instance), engine(engine), id(id), class_name(class_name), name(instance.name)
{}

void ModuleCreator::add_audio_input(uint8_t channels)
{
    instance.input_audio_ports.push_back(AudioEngine::ModuleAudioPort(
        channels,
        0,
        0
    ));
}

void ModuleCreator::add_audio_output(uint8_t channels)
{
    instance.output_audio_ports.push_back(AudioEngine::ModuleAudioPort(
        channels,
        0,
        0
    ));
}

void ModuleCreator::add_message_input()
{
    instance.input_message_ports.push_back(AudioEngine::ModuleMessagePort
    {
        0,
        0
    });
}

void ModuleCreator::add_message_output()
{
    instance.output_message_ports.push_back(AudioEngine::ModuleMessagePort
    {
        0,
        0
    });
}

template <>
AudioEngine::ModuleControl ModuleCreator::_create_module_control<float>(const std::string &name, float default_value)
{
    AudioEngine::ModuleControl ctl;
    ctl.name = name;
    ctl.data_type = ModuleControlDataType::FLOAT;
    ctl.float_value = default_value;
    return ctl;
}

template <>
AudioEngine::ModuleControl ModuleCreator::_create_module_control<double>(const std::string &name, double default_value)
{
    AudioEngine::ModuleControl ctl;
    ctl.name = name;
    ctl.data_type = ModuleControlDataType::DOUBLE;
    ctl.double_value = default_value;
    return ctl;
}

template <>
AudioEngine::ModuleControl ModuleCreator::_create_module_control<std::int32_t>(const std::string &name, std::int32_t default_value)
{
    AudioEngine::ModuleControl ctl;
    ctl.name = name;
    ctl.data_type = ModuleControlDataType::INT32;
    ctl.int32_value = default_value;
    return ctl;
}

template <>
AudioEngine::ModuleControl ModuleCreator::_create_module_control<std::int64_t>(const std::string &name, std::int64_t default_value)
{
    AudioEngine::ModuleControl ctl;
    ctl.name = name;
    ctl.data_type = ModuleControlDataType::INT64;
    ctl.int64_value = default_value;
    return ctl;
}

template <>
AudioEngine::ModuleControl ModuleCreator::_create_module_control<bool>(const std::string &name, bool default_value)
{
    AudioEngine::ModuleControl ctl;
    ctl.name = name;
    ctl.data_type = ModuleControlDataType::BOOL;
    ctl.bool_value = default_value;
    return ctl;
}








/////////////////////////////
// MODULE PROCESSOR HANDLE //
/////////////////////////////

ModuleProcessor::ModuleProcessor(
    size_t buffer_frame_count, unsigned long frame_time, unsigned int sample_rate,
    AudioEngine::ModuleGraph *graph, ModuleID id
) :
    graph(graph),
    node(graph->nodes[id]),
    buffer_frame_count(buffer_frame_count),
    frame_time(frame_time),
    sample_rate(sample_rate),
    class_name(node.module->class_name.c_str()),
    userdata(node.module->userdata)
{}

float* ModuleProcessor::audio_input(unsigned int index) const
{
    assert(index < node.audio_inputs.size() || node.module->is_stereo_mixer);
    if (index >= node.audio_inputs.size()) return nullptr;

    const auto& cn = node.audio_inputs[index];
    
    // unconnected node, send zero-filled buffer
    if (cn.index == -1)
    {
        return node.module->audio_data.input_dummy_buffer;
    }

    return graph->nodes[node.dependencies[cn.index]].module->audio_data.output_audio_buffers[cn.from_port];
}

float* ModuleProcessor::audio_output(unsigned int index) const
{
    assert(index < node.audio_outputs.size());
    if (index >= node.module->output_audio_ports.size()) return nullptr;
    return node.module->audio_data.output_audio_buffers[index];
}

std::uint8_t ModuleProcessor::audio_input_channels(unsigned int index) const
{
    assert(index < node.audio_inputs.size());
    return node.module->input_audio_ports[index].channel_count;
}

std::uint8_t ModuleProcessor::audio_output_channels(unsigned int index) const
{
    assert(index < node.audio_outputs.size());
    return node.module->output_audio_ports[index].channel_count;
}

unsigned int ModuleProcessor::read_message(unsigned int index, void *buffer, unsigned int max_length)
{
    AudioEngine::ModuleInstance &mod = *node.module;

    assert(index < mod.input_message_ports.size());
    if (index >= mod.input_message_ports.size()) return 0;

    AudioEngine::MessageHeader msg_header;
    auto &queue = mod.audio_data.input_messages[index];
    
    if (queue.available_for_read() < sizeof(msg_header)) return 0;
    queue.read((std::byte*) &msg_header, sizeof(msg_header));

    // can't fit data into buffer, discard message...
    if (msg_header.size > max_length)
    {
        queue.discard(msg_header.size);
        return 0;
    }

    queue.read((std::byte*) buffer, msg_header.size);
    return msg_header.size;
}

bool ModuleProcessor::send_message(unsigned int index, void *data, unsigned int data_size)
{
    AudioEngine::ModuleInstance &mod = *node.module;
    assert(index < node.message_outputs.size());
    if (index >= node.message_outputs.size()) return false;

    auto &cn = node.message_outputs[index];
    if (cn.index == -1) return false;

    if (data_size == 0) return true;

    AudioEngine::MessageHeader msg_header { data_size };

    auto &target_module = graph->nodes[node.dependents[cn.index]].module;
    if (target_module == nullptr) {
        logger::log_debug("ModuleProcessor::send_message: send to module that has no effect!");
        return false;
    }

    auto &queue = target_module->audio_data.input_messages[cn.to_port];
    /*assert(index < mod.output_message_ports.size());
    if (index >= mod.output_message_ports.size()) return false;
    if (data_size == 0) return true;

    AudioEngine::MessageHeader msg_header { data_size };
    
    auto &queue = mod.audio_data.output_messages[index];*/
    if (queue.available_for_write() < sizeof(msg_header) + data_size)
        return false;

    queue.write((std::byte*) &msg_header, sizeof(msg_header));
    queue.write((std::byte*) data, data_size);
    return true;
}