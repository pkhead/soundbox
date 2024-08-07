#include "audio_engine.hpp"
#include <cassert>

using namespace modules;








///////////////////////////
// MODULE CREATOR HANDLE //
///////////////////////////

ModuleCreator::ModuleCreator(ModuleID id, AudioEngine& engine, std::string class_name, AudioEngine::ModuleInstance& instance) :
    instance(instance), engine(engine), id(id), class_name(class_name)
{}

void ModuleCreator::add_audio_input(uint8_t channels)
{
    instance.input_audio_ports.push_back(AudioEngine::ModuleAudioPort
    {
        .channel_count = channels,
        .connected_module = 0,
        .connection_port = 0
    });
}

void ModuleCreator::add_audio_output(uint8_t channels)
{
    instance.output_audio_ports.push_back(AudioEngine::ModuleAudioPort
    {
        .channel_count = channels,
        .connected_module = 0,
        .connection_port = 0
    });
}

void ModuleCreator::add_message_input()
{
    instance.input_message_ports.push_back(AudioEngine::ModuleMessagePort
    {
        .connected_module = 0,
        .connection_port = 0
    });
}

void ModuleCreator::add_message_output()
{
    instance.output_message_ports.push_back(AudioEngine::ModuleMessagePort
    {
        .connected_module = 0,
        .connection_port = 0
    });
}

template <>
AudioEngine::ModuleControl ModuleCreator::_create_module_control<float>(const std::string &name, float default_value)
{
    return AudioEngine::ModuleControl
    {
        .name = name,
        .data_type = ModuleControlDataType::FLOAT,
        .float_value = default_value
    };
}

template <>
AudioEngine::ModuleControl ModuleCreator::_create_module_control<double>(const std::string &name, double default_value)
{
    return AudioEngine::ModuleControl
    {
        .name = name,
        .data_type = ModuleControlDataType::DOUBLE,
        .double_value = default_value
    };
}

template <>
AudioEngine::ModuleControl ModuleCreator::_create_module_control<std::int32_t>(const std::string &name, std::int32_t default_value)
{
    return AudioEngine::ModuleControl
    {
        .name = name,
        .data_type = ModuleControlDataType::INT32,
        .int32_value = default_value
    };
}

template <>
AudioEngine::ModuleControl ModuleCreator::_create_module_control<std::int64_t>(const std::string &name, std::int64_t default_value)
{
    return AudioEngine::ModuleControl
    {
        .name = name,
        .data_type = ModuleControlDataType::INT64,
        .int64_value = default_value
    };
}

template <>
AudioEngine::ModuleControl ModuleCreator::_create_module_control<bool>(const std::string &name, bool default_value)
{
    return AudioEngine::ModuleControl
    {
        .name = name,
        .data_type = ModuleControlDataType::BOOL,
        .bool_value = default_value
    };
}








/////////////////////////////
// MODULE PROCESSOR HANDLE //
/////////////////////////////

ModuleProcessor::ModuleProcessor(size_t buffer_frame_count, unsigned int sample_rate, AudioEngine::ModuleGraphNode* node) :
    node(node),
    buffer_frame_count(buffer_frame_count),
    sample_rate(sample_rate),
    class_name(node->module->class_name.c_str()),
    userdata(node->module->userdata)
{}

float* ModuleProcessor::audio_input(unsigned int index) const
{
    assert(index < node->module->input_audio_ports.size() || node->module->is_stereo_mixer);
    if (index >= node->audio_inputs.size()) return nullptr;

    const AudioEngine::ModuleGraphConnection& cn = node->audio_inputs[index];
    
    // unconnected node, send zero-filled buffer
    if (cn.from_node_index == -1)
    {
        return node->module->audio_data.input_dummy_buffer;
    }

    return node->dependencies[cn.from_node_index]->module->audio_data.output_audio_buffers[cn.from_port];
}

float* ModuleProcessor::audio_output(unsigned int index) const
{
    assert(index < node->module->output_audio_ports.size());
    if (index >= node->module->output_audio_ports.size()) return nullptr;
    return node->module->audio_data.output_audio_buffers[index];
}

std::uint8_t ModuleProcessor::audio_input_channels(unsigned int index) const
{
    assert(index < node->module->input_audio_ports.size());
    return node->module->input_audio_ports[index].channel_count;
}

std::uint8_t ModuleProcessor::audio_output_channels(unsigned int index) const
{
    assert(index < node->module->output_audio_ports.size());
    return node->module->output_audio_ports[index].channel_count;
}

unsigned int ModuleProcessor::read_message(unsigned int index, void *buffer, unsigned int max_length)
{
    AudioEngine::ModuleInstance &mod = *node->module;

    assert(index < mod.input_message_ports.size());
    if (index >= mod.input_message_ports.size()) return 0;

    AudioEngine::MessageHeader msg_header;
    auto &queue = mod.audio_data.input_messages[index];
    
    if (queue.available_for_read() < sizeof(msg_header)) return 0;
    queue.read((std::byte*) &msg_header, sizeof(msg_header));

    // can't fit data into buffer
    if (msg_header.size > max_length) return 0;

    queue.read((std::byte*) buffer, msg_header.size);
    return msg_header.size;
}

bool ModuleProcessor::send_message(unsigned int index, void *data, unsigned int data_size)
{
    AudioEngine::ModuleInstance &mod = *node->module;

    assert(index < mod.output_message_ports.size());
    if (index >= mod.output_message_ports.size()) return false;
    if (data_size == 0) return true;

    AudioEngine::MessageHeader msg_header
    {
        .size = data_size
    };
    
    auto &queue = mod.audio_data.output_messages[index];
    if (queue.available_for_write() < sizeof(msg_header) + data_size)
        return false;

    queue.write((std::byte*) &msg_header, sizeof(msg_header));
    queue.write((std::byte*) data, data_size);
    return true;
}