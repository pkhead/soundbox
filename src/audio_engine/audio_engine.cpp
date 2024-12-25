#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <cmath>
#include <cassert>
#include <thread>
#include <imgui.h>
#include <portaudio.h>
#include <sys.hpp>
#include <unordered_map>
#include "audio_engine.hpp"
#include "../log.hpp"

using namespace modules;

constexpr size_t MESSAGE_PORT_CAPACITY = 512;
const char* AudioEngine::MODULE_CLASS_AUDIO_OUT = "AUDIO_OUT";
const char* AudioEngine::MODULE_CLASS_STEREO_MIXER = "STEREO_MIXER";
const char* AudioEngine::MODULE_CLASS_MESSAGE_DUPLICATOR = "MESSAGE_DUPLICATOR";

ModuleID AudioEngine::_next_module_id = 1;

static void pa_panic(PaError err)
{
    logger::log_error("PortAudio Error: %s", Pa_GetErrorText(err));

    PaError stop_err = Pa_Terminate();
    if (stop_err != paNoError)
        logger::log_error("PortAudio Error: %s", Pa_GetErrorText(err));

    throw std::runtime_error("portaudio error");
}

AudioEngine::AudioEngine() :
    _audio_ring_buffer(4096),
    _frames_per_buffer(256)
{
    _frame_time = 0;
    _is_graph_dirty = true;

    // initialize port audio
    PaError err = Pa_Initialize();
    if (err != paNoError)
    {
        logger::log_error("Could not start PortAudio %s", Pa_GetErrorText(err));
        throw std::runtime_error("could not initialize audio engine");
    }

    logger::log_info("PortAudio available backends:");

    int selected_backend = Pa_GetDefaultHostApi();

    for (int i = 0; i < Pa_GetHostApiCount(); i++)
    {
        const PaHostApiInfo* info = Pa_GetHostApiInfo(i);

        std::stringstream stream;

        if (selected_backend == i)
            stream << "  [x] - ";
        else
            stream << "  [ ] - ";
        
        stream << info->name;
        logger::log_info("%s", stream.str().c_str());
    }

    int output_device = -1;

    // if output_device == -1, use default output device
    if (output_device == -1 && (output_device = Pa_GetDefaultOutputDevice()) == paNoDevice)
    {
        logger::log_warning("no default output device found");
        output_device = 0;
    }

    _sample_rate = 44100;

    PaStreamParameters out_params;
    out_params.channelCount = 2;
    out_params.device = output_device;
    out_params.hostApiSpecificStreamInfo = nullptr;
    out_params.sampleFormat = paFloat32;
    out_params.suggestedLatency = 0.08;
    err = Pa_OpenStream(
        &_pa_stream,
        nullptr,
        &out_params, // num output channels (stereo)
        _sample_rate, // sample rate
        _frames_per_buffer, // num frames per buffer (am using own buffer so this is not needed)
        0, // stream flags
        _pa_stream_callback, // callback function
        (void*)this // user data
    );
    if (err != paNoError) pa_panic(err);

    err = Pa_StartStream(_pa_stream);
    if (err != paNoError) pa_panic(err);

    _output_channels = out_params.channelCount;
    assert(_output_channels == 2);

    _audio_buffer = std::vector<float>(_frames_per_buffer * _output_channels);
    _is_engine_runnning = true;
    _thread = std::thread(&AudioEngine::_thread_process, this);
}

AudioEngine::~AudioEngine()
{
    _is_engine_runnning = false;
    _thread.join();

    if (_pa_stream == nullptr) return;

    PaError err = Pa_StopStream(_pa_stream);
    if (err != paNoError)
        logger::log_error("PortAudio Error: %s", Pa_GetErrorText(err));

    PaError stop_err = Pa_Terminate();
    if (stop_err != paNoError)
        logger::log_error("PortAudio Error: %s", Pa_GetErrorText(err));
}

int AudioEngine::_pa_stream_callback(
    const void* input_buffer,
    void* output_buffer,
    unsigned long frame_count,
    const PaStreamCallbackTimeInfo* time_info,
    PaStreamCallbackFlags status_flags,
    void* userdata
)
{
    AudioEngine* self = (AudioEngine*) userdata;

    //logger::log_debug("available to read: %lu", self->_audio_ring_buffer.available_for_read());

    float* out_samples = (float*) output_buffer;
    if (!self->_audio_ring_buffer.read(out_samples, frame_count * self->_output_channels))
    {
#ifdef DEBUG
        logger::log_debug("AudioEngine::_pa_stream_callback: not enough audio data to fill buffer");
#endif
        memset(out_samples, 0, frame_count * self->_output_channels * sizeof(float));
    }

    return 0;
}

bool AudioEngine::register_host(std::unique_ptr<ModuleHost> &&host)
{
    if (!host->initialize()) return false;

    for (auto& mod_class : host->scan_modules())
    {
        _available_module_classes.push_back(mod_class);
    }

    logger::log_info("register host '%s'", host->host_id());
    _hosts[host->host_id()] = std::move(host);
    return true;
}

static std::string get_host_id(const std::string &mod_class)
{
    size_t sep_index = mod_class.find_first_of("::");
    if (sep_index == std::string::npos) return ""; // module id had no :: separator, return unknown module

    std::string host_id = mod_class.substr(0, sep_index);
    return host_id;
}

ModuleID AudioEngine::create_module(const std::string &mod_class)
{
    ModuleID this_id = _next_module_id;
    std::shared_ptr<ModuleInstance> instance = std::make_shared<ModuleInstance>();
    instance->class_name = mod_class;

    instance->is_stereo_mixer = false;
    instance->is_message_duplicator = false;
    instance->processor = nullptr;
    instance->idle = nullptr;
    instance->userdata = nullptr;

    if (mod_class == MODULE_CLASS_AUDIO_OUT)
    {
        instance->name = "Audio Output";
        instance->input_audio_ports.resize(1);

        for (unsigned int i = 0; i < 1; i++)
        {
            instance->input_audio_ports[i] = ModuleAudioPort(
                (uint8_t) _output_channels,
                0,
                0
            );
        }

        instance->userdata = this;
        instance->processor = _s_process_audio_out_node;
    }
    else if (mod_class == MODULE_CLASS_STEREO_MIXER)
    {
        instance->name = "Stereo Mixer";
        instance->output_audio_ports.resize(1);
        instance->output_audio_ports[0] = ModuleAudioPort(
            2,
            0,
            0
        );

        instance->is_stereo_mixer = true;
        instance->processor = _s_process_stereo_mixer_node;
    }
    else if (mod_class == MODULE_CLASS_MESSAGE_DUPLICATOR)
    {
        instance->name = "Message Duplicator";
        instance->input_message_ports.resize(1);
        instance->input_message_ports[0] = ModuleMessagePort(
            0,
            0
        );

        instance->is_message_duplicator = true;
        instance->processor = _s_process_message_duplicator_node;
    }
    else
    {
        modules::ModuleInfo *mod_class_info = nullptr;
        for (auto &v : _available_module_classes)
        {
            if (v.class_name == mod_class)
            {
                mod_class_info = &v;
            }
        }

        if (mod_class_info == nullptr)
        {
            logger::log_warning("module '%s' could not be created: class not recognized", mod_class.c_str());
            return 0;
        }

        instance->name = mod_class_info->name;

        // get host id from module id
        std::string host_id = get_host_id(mod_class);
        auto it = _hosts.find(host_id);
        if (it == _hosts.end())
        {
            logger::log_warning("module '%s' could not be created: host not found", mod_class.c_str());
            return 0;
        }

        ModuleHost& host = *(it->second);

        // ask host to setup module data
        ModuleCreator creator(this_id, *this, mod_class, *instance);

        if (!host.create_module(creator))
        {
            logger::log_warning("module '%s' could not be created", mod_class.c_str());
            return 0;
        }
        
        if (creator.processor == nullptr)
        {
            logger::log_warning("no processor for module class %s", mod_class.c_str());
            return 0;
        }

        instance->userdata = creator.userdata;
        instance->processor = creator.processor;
        instance->idle = creator.idle;
    }

    _next_module_id++;

    // generate audio/message buffers
    for (auto it = instance->output_audio_ports.begin(); it != instance->output_audio_ports.end(); it++)
    {
        instance->audio_data.output_audio_buffers.push_back(new float[_frames_per_buffer * it->channel_count]);
    }

    for (auto it = instance->input_message_ports.begin(); it != instance->input_message_ports.end(); it++)
    {
        instance->audio_data.input_messages.push_back(RingBuffer<std::byte>(MESSAGE_PORT_CAPACITY));
    }

    for (auto it = instance->output_message_ports.begin(); it != instance->output_message_ports.end(); it++)
    {
        instance->audio_data.output_messages.push_back(RingBuffer<std::byte>(MESSAGE_PORT_CAPACITY));
    }

    // generate dummy input buffer, for if an input isn't connected to anything
    std::uint8_t max_input_channel_count = 0;
    for (auto it = instance->input_audio_ports.begin(); it != instance->input_audio_ports.end(); it++)
    {
        if (it->channel_count > max_input_channel_count)
            max_input_channel_count = it->channel_count;
    }

    if (max_input_channel_count > 0)
    {
        instance->audio_data.input_dummy_buffer = new float[_frames_per_buffer * max_input_channel_count];
        memset(instance->audio_data.input_dummy_buffer, 0, _frames_per_buffer * max_input_channel_count * sizeof(float));
    }
    else
    {
        instance->audio_data.input_dummy_buffer = nullptr;
    }

    _modules[this_id] = instance;
    return this_id;
}

void AudioEngine::destroy_module(ModuleID mod_id)
{
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end()) return;

    std::shared_ptr<ModuleInstance>& mod = it->second;

    for (std::size_t i = 0; i < mod->input_audio_ports.size(); i++)
    {
        disconnect_audio_input(mod_id, i);
    }

    for (std::size_t i = 0; i < mod->output_audio_ports.size(); i++)
    {
        disconnect_audio_output(mod_id, i);
    }

    // defer calling destroy_module until after update has been called
    // and the newly updated audio graph, with the module absent, has been
    // sent to the audio process.
    _destroy_queue.push_back(DestroyQueueItem
    {
        mod_id,
        std::move(mod)
    });
    _modules.erase(mod_id);
}

std::vector<ModuleID> AudioEngine::list_modules() const
{
    std::vector<ModuleID> modules;
    for (auto &[id, instance] : _modules)
        modules.push_back(id);

    return modules;
}

bool AudioEngine::module_exists(ModuleID mod_id) const
{
    if (mod_id == 0) return false;
    const auto &it = _modules.find(mod_id);
    return it != _modules.end();
}

static std::string EMPTY_STRING = "";

const std::string& AudioEngine::module_class_name(ModuleID mod_id) const
{
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end())
        return EMPTY_STRING;
    
    const ModuleInstance &mod = *it->second;
    return mod.class_name;
}

const std::string& AudioEngine::module_name(ModuleID mod_id) const
{
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end())
        return EMPTY_STRING;
    
    const ModuleInstance &mod = *it->second;
    return mod.name;
}







///////////////////////////////
// MODULE AUDIO INPUT/OUTPUT //
///////////////////////////////

unsigned int AudioEngine::audio_input_count(ModuleID mod_id) const
{
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end())
        return 0;
    
    const ModuleInstance &mod = *it->second;
    return mod.input_audio_ports.size();
}

unsigned int AudioEngine::audio_output_count(ModuleID mod_id) const
{
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end())
        return 0;
    
    const ModuleInstance &mod = *it->second;
    return mod.output_audio_ports.size();
}

std::uint8_t AudioEngine::audio_input_channel_count(ModuleID mod_id, unsigned int index) const
{
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end())
        return 0;

    const ModuleInstance &mod = *it->second;

    // from outsider's perspectives, the stereo mixer module only has one input port. 
    if (mod.is_stereo_mixer && index > 0) return 0;

    if (index >= mod.input_audio_ports.size())
        return 0;

    return mod.input_audio_ports[index].channel_count;
}

std::uint8_t AudioEngine::audio_output_channel_count(ModuleID mod_id, unsigned int index) const
{
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end())
        return 0;

    const ModuleInstance &mod = *it->second;
    if (index >= mod.output_audio_ports.size())
        return 0;

    return mod.output_audio_ports[index].channel_count;
}

void AudioEngine::get_audio_input_connection(ModuleID mod_a, unsigned int in_index, ModuleID &mod_b, unsigned int &out_index) const
{
    const auto &it = _modules.find(mod_a);
    if (it == _modules.end())
    {
        mod_b = 0;
        out_index = 0;
        return;
    }

    const ModuleInstance &mod = *it->second;
    
    // stereo mixers may have more than one connected into its input port...
    if (mod.is_stereo_mixer)
    {
        mod_b = 0;
        out_index = 0;
        return;
    }

    if (in_index >= mod.input_audio_ports.size())
    {
        mod_b = 0;
        out_index = 0;
        return;
    }

    mod_b = mod.input_audio_ports[in_index].connected_module;
    out_index = mod.input_audio_ports[in_index].connection_port;
}

void AudioEngine::get_audio_output_connection(ModuleID mod_a, unsigned int out_index, ModuleID &mod_b, unsigned int &in_index) const
{
    const auto &it = _modules.find(mod_a);
    if (it == _modules.end())
    {
        mod_b = 0;
        in_index = 0;
        return;
    }

    const ModuleInstance &mod = *it->second;
    if (out_index >= mod.output_audio_ports.size())
    {
        mod_b = 0;
        in_index = 0;
        return;
    }

    mod_b = mod.output_audio_ports[out_index].connected_module;
    in_index = mod.output_audio_ports[out_index].connection_port;
}

bool AudioEngine::connect_audio(ModuleID mod_a_id, ModuleID mod_b_id, unsigned int out_index, unsigned int in_index)
{
    logger::log_info(
        "connect audio %s output %i to %s input %i",
        module_name(mod_a_id).c_str(),
        out_index,
        module_name(mod_b_id).c_str(),
        in_index
    );

    const auto &it_a = _modules.find(mod_a_id);
    if (it_a == _modules.end()) return false;

    const auto &it_b = _modules.find(mod_b_id);
    if (it_b == _modules.end()) return false;

    ModuleInstance &mod_a = *it_a->second;
    ModuleInstance &mod_b = *it_b->second;

    if (out_index >= mod_a.output_audio_ports.size()) return false;

    if (mod_b.is_stereo_mixer)
    {
        if (mod_a.output_audio_ports[out_index].channel_count != 2) return false;
        disconnect_audio_output(mod_a_id, out_index);

        mod_a.output_audio_ports[out_index].connected_module = mod_b_id;
        mod_a.output_audio_ports[out_index].connection_port = 0;

        mod_b.input_audio_ports.push_back(ModuleAudioPort(
            2,
            mod_a_id,
            out_index
        ));
    }
    else
    {
        if (in_index >= mod_b.input_audio_ports.size()) return false;
        if (mod_a.output_audio_ports[out_index].channel_count != mod_b.input_audio_ports[in_index].channel_count) return false;

        disconnect_audio_output(mod_a_id, out_index);
        disconnect_audio_input(mod_b_id, in_index);

        mod_a.output_audio_ports[out_index].connected_module = mod_b_id;
        mod_a.output_audio_ports[out_index].connection_port = in_index;

        mod_b.input_audio_ports[in_index].connected_module = mod_a_id;
        mod_b.input_audio_ports[in_index].connection_port = out_index;
    }

    _is_graph_dirty = true;
    return true;
}

bool AudioEngine::disconnect_audio_output(ModuleID mod_id, unsigned int out_index)
{
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end()) return false;

    ModuleInstance &mod = *it->second;
    if (out_index >= mod.output_audio_ports.size()) return false;

    if (mod.output_audio_ports[out_index].connected_module != 0)
    {
        _is_graph_dirty = true;
        ModuleInstance &connected = *_modules.at(mod.output_audio_ports[out_index].connected_module);

        if (connected.is_stereo_mixer)
        {
            for (auto it = connected.input_audio_ports.begin(); it != connected.input_audio_ports.end(); it++)
            {
                if (it->connected_module == mod_id)
                {
                    connected.input_audio_ports.erase(it);
                    break;
                }
            }
        }
        else
        {
            unsigned int in_index = mod.output_audio_ports[out_index].connection_port;
            connected.input_audio_ports[in_index].connected_module = 0;
            connected.input_audio_ports[in_index].connection_port = 0;
        }
    }

    mod.output_audio_ports[out_index].connection_port = 0;
    mod.output_audio_ports[out_index].connected_module = 0;

    return true;
}

bool AudioEngine::disconnect_audio_input(ModuleID mod_id, unsigned int in_index)
{
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end()) return false;

    ModuleInstance &mod = *it->second;

    if (mod.is_stereo_mixer)
    {
        if (in_index > 0) return false;

        for (auto &cn : mod.input_audio_ports)
        {
            _is_graph_dirty = true;
            ModuleInstance &connected = *_modules.at(cn.connected_module);
            connected.output_audio_ports[cn.connection_port].connected_module = 0;
            connected.output_audio_ports[cn.connection_port].connection_port = 0;
        }

        mod.input_audio_ports.clear();
    }
    else
    {
        if (in_index >= mod.input_audio_ports.size()) return false;

        if (mod.input_audio_ports[in_index].connected_module != 0)
        {
            _is_graph_dirty = true;
            ModuleInstance &connected = *_modules.at(mod.input_audio_ports[in_index].connected_module);
            unsigned int out_index = mod.input_audio_ports[in_index].connection_port;
            connected.output_audio_ports[out_index].connected_module = 0;
            connected.output_audio_ports[out_index].connection_port = 0;
        }

        mod.input_audio_ports[in_index].connection_port = 0;
        mod.input_audio_ports[in_index].connected_module = 0;
    }

    return true;
}







//////////////////////////////////////
// MODULE MESSAGE PORT INPUT/OUTPUT //
//////////////////////////////////////
unsigned int AudioEngine::message_input_count(ModuleID mod_id) const
{
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end())
        return 0;
    
    const ModuleInstance &mod = *it->second;
    return mod.input_message_ports.size();
}

unsigned int AudioEngine::message_output_count(ModuleID mod_id) const
{
    const auto& it = _modules.find(mod_id);
    if (it == _modules.end())
        return 0;

    const ModuleInstance &mod = *it->second;
    return mod.output_message_ports.size();
}

void AudioEngine::get_message_input_connection(ModuleID mod_a, unsigned int in_index, ModuleID &mod_b, unsigned int &out_index) const
{
    const auto &it = _modules.find(mod_a);
    if (it == _modules.end())
    {
        mod_b = 0;
        out_index = 0;
        return;
    }

    const ModuleInstance &mod = *it->second;
    if (in_index >= mod.input_message_ports.size())
    {
        mod_b = 0;
        out_index = 0;
        return;
    }

    mod_b = mod.input_message_ports[in_index].connected_module;
    out_index = mod.input_message_ports[in_index].connection_port;
}

void AudioEngine::get_message_output_connection(ModuleID mod_a, unsigned int out_index, ModuleID &mod_b, unsigned int &in_index) const
{
    const auto &it = _modules.find(mod_a);
    if (it == _modules.end())
    {
        mod_b = 0;
        in_index = 0;
        return;
    }

    const ModuleInstance &mod = *it->second;
    if (out_index >= mod.output_message_ports.size())
    {
        mod_b = 0;
        in_index = 0;
        return;
    }

    mod_b = mod.output_message_ports[out_index].connected_module;
    in_index = mod.output_message_ports[out_index].connection_port;
}

bool AudioEngine::connect_message(ModuleID mod_a_id, ModuleID mod_b_id, unsigned int out_index, unsigned int in_index)
{
    logger::log_info(
        "connect msg %s output %i to %s input %i",
        module_name(mod_a_id).c_str(),
        out_index,
        module_name(mod_b_id).c_str(),
        in_index
    );

    const auto &it_a = _modules.find(mod_a_id);
    if (it_a == _modules.end()) return false;

    const auto &it_b = _modules.find(mod_b_id);
    if (it_b == _modules.end()) return false;

    ModuleInstance &mod_a = *it_a->second;
    ModuleInstance &mod_b = *it_b->second;

    if (in_index >= mod_b.input_message_ports.size()) return false;

    if (mod_a.is_message_duplicator) {
        if (out_index > 0) return false;

        disconnect_message_input(mod_b_id, in_index);

        mod_a.output_message_ports.push_back(ModuleMessagePort(
            mod_b_id,
            in_index
        ));

        mod_b.input_message_ports[in_index].connected_module = mod_a_id;
        mod_b.input_message_ports[in_index].connection_port = 0;
    } else {
        if (out_index >= mod_a.output_message_ports.size()) return false;

        disconnect_message_output(mod_a_id, out_index);
        disconnect_message_input(mod_b_id, in_index);

        mod_a.output_message_ports[out_index].connected_module = mod_b_id;
        mod_a.output_message_ports[out_index].connection_port = in_index;

        mod_b.input_message_ports[in_index].connected_module = mod_a_id;
        mod_b.input_message_ports[in_index].connection_port = out_index;
    }

    _is_graph_dirty = true;
    return true;
}

bool AudioEngine::disconnect_message_output(ModuleID mod_id, unsigned int out_index)
{
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end()) return false;

    ModuleInstance &mod = *it->second;

    if (mod.is_message_duplicator) {
        if (out_index > 0) return false;

        for (auto &port : mod.output_message_ports) {
            if (port.connected_module == 0) continue;
            _is_graph_dirty = true;

            ModuleInstance &connected = *_modules.at(port.connected_module);
            connected.input_message_ports[port.connection_port].connected_module = 0;
            connected.input_message_ports[port.connection_port].connection_port = 0;
        }

        mod.output_message_ports.clear();
    } else {
        if (out_index >= mod.output_message_ports.size()) return false;

        if (mod.output_message_ports[out_index].connected_module != 0)
        {
            _is_graph_dirty = true;
            ModuleInstance &connected = *_modules.at(mod.output_message_ports[out_index].connected_module);
            unsigned int in_index = mod.output_message_ports[out_index].connection_port;
            connected.input_message_ports[in_index].connected_module = 0;
            connected.input_message_ports[in_index].connection_port = 0;
        }

        mod.output_message_ports[out_index].connection_port = 0;
        mod.output_message_ports[out_index].connected_module = 0;
    }

    return true;
}

bool AudioEngine::disconnect_message_input(ModuleID mod_id, unsigned int in_index)
{
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end()) return false;

    ModuleInstance &mod = *it->second;
    if (in_index >= mod.input_message_ports.size()) return false;

    if (mod.input_message_ports[in_index].connected_module != 0)
    {
        ModuleInstance &connected = *_modules.at(mod.input_message_ports[in_index].connected_module);

        if (connected.is_message_duplicator) {
            for (auto it = connected.output_message_ports.begin(); it != connected.output_message_ports.end(); it++) {
                if (it->connected_module == mod_id) {
                    _is_graph_dirty = true;
                    connected.output_message_ports.erase(it);
                    break;
                }
            }
        } else {
            _is_graph_dirty = true;
            unsigned int out_index = mod.input_message_ports[in_index].connection_port;
            connected.output_message_ports[out_index].connected_module = 0;
            connected.output_message_ports[out_index].connection_port = 0;
        }
    }

    mod.input_message_ports[in_index].connection_port = 0;
    mod.input_message_ports[in_index].connected_module = 0;

    return true;
}

/*bool AudioEngine::send_message(ModuleID mod_id, unsigned int index, void* data, unsigned int data_size)
{
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end()) return false;
    ModuleInstance &mod = *it->second;

    assert(index < mod.input_message_ports.size());
    if (index >= mod.input_message_ports.size()) return false;
    if (data_size == 0) return true;

    MessageHeader msg_header
    {
        .size = data_size
    };
    
    auto &queue = mod.audio_data.input_messages[index];
    if (queue.available_for_write() < sizeof(msg_header) + data_size)
        return false;

    queue.write((std::byte*) &msg_header, sizeof(msg_header));
    queue.write((std::byte*) data, data_size);
    return true;
}*/

unsigned int AudioEngine::control_count(ModuleID mod_id) const
{
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end()) return false;
    const ModuleInstance &mod = *it->second;

    return mod.controls.size();
}

ModuleControlDataType AudioEngine::control_data_type(ModuleID mod_id, unsigned int index) const
{
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end()) return ModuleControlDataType::UNKNOWN;
    const ModuleInstance &mod = *it->second;

    if (index >= mod.controls.size()) return ModuleControlDataType::UNKNOWN;
    return mod.controls[index].data_type;
}

const std::string AudioEngine::control_name(ModuleID mod_id, unsigned int index) const
{
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end()) return "";
    const ModuleInstance &mod = *it->second;
    if (index >= mod.controls.size()) return "";

    return mod.controls[index].name;
}

template <>
bool AudioEngine::_control_get_ref<float>(ModuleControl &control, float** v)
{
    if (control.data_type != ModuleControlDataType::FLOAT) return false;
    *v = &control.float_value;
    return true;
}

template <>
bool AudioEngine::_control_get_ref<double>(ModuleControl &control, double** v)
{
    if (control.data_type != ModuleControlDataType::DOUBLE) return false;
    *v = &control.double_value;
    return true;
}

template <>
bool AudioEngine::_control_get_ref<std::int32_t>(ModuleControl &control, std::int32_t** v)
{
    if (control.data_type != ModuleControlDataType::INT32) return false;
    *v = &control.int32_value;
    return true;
}

template <>
bool AudioEngine::_control_get_ref<std::int64_t>(ModuleControl &control, std::int64_t** v)
{
    if (control.data_type != ModuleControlDataType::INT64) return false;
    *v = &control.int64_value;
    return true;
}

template <>
bool AudioEngine::_control_get_ref<bool>(ModuleControl &control, bool** v)
{
    if (control.data_type != ModuleControlDataType::BOOL) return false;
    *v = &control.bool_value;
    return true;
}

void AudioEngine::update()
{
    // call module idle processes
    for (auto& [ id, inst ] : _modules)
    {
        if (inst->idle == nullptr) continue;
        inst->idle(*this, id, inst->userdata);
    }
    
    if (!_is_graph_dirty) return;

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

        std::shared_ptr<ModuleInstance>& inst = _modules.at(id);

        // parse dependencies
        unsigned int input_port = 0;
        for (auto it = inst->input_audio_ports.begin(); it != inst->input_audio_ports.end(); it++) {
            if (module_exists(it->connected_module)) {
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
            if (module_exists(it->connected_module)) {
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
            if (module_exists(it->connected_module)) {
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
            if (module_exists(it->connected_module)) {
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
    
    std::unique_ptr<ModuleGraph> new_graph = nullptr;

    // find the AUDIO_OUT class to call build_graph
    for (auto &[ id, inst ] : _modules)
    {
        if (inst->class_name == MODULE_CLASS_AUDIO_OUT)
        {
            build_graph(id, 0);

            new_graph = std::make_unique<ModuleGraph>();
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
                    _modules.at(id),
                    std::move(info.dependencies),
                    std::move(info.dependents),
                    std::move(info.audio_inputs),
                    std::move(info.audio_outputs),
                    std::move(info.message_inputs),
                    std::move(info.message_outputs)
                };
            }

            break;
        }
    }

    _mutex.lock();
    _current_graph = std::move(new_graph);
    _mutex.unlock();

    // flush destroy queue
    for (auto &item : _destroy_queue)
    {
        std::string host_id = get_host_id(item.module->class_name);
        const auto &host_it = _hosts.find(host_id);
        if (host_it == _hosts.end())
        {
            logger::log_warning("module %s could not be destroyed: could not find host", item.module->class_name.c_str());
            continue;
        }

        std::unique_ptr<ModuleHost> &host = host_it->second;
        host->destroy_module(item.module->class_name, item.id, item.module->userdata);
    }
    _destroy_queue.clear();

    _is_graph_dirty = false;
}

void AudioEngine::_process_node(ModuleID id)
{
    auto &node = _current_graph->nodes[id];

    // call processor
    ModuleProcessor processor(_frames_per_buffer, _frame_time, _sample_rate, _current_graph.get(), id);
    assert(node.module->processor != nullptr);
    node.module->processor(processor);
}

void AudioEngine::_s_process_audio_out_node(ModuleProcessor& process)
{
    AudioEngine* self = (AudioEngine*) process.userdata;
    self->_process_audio_out_node(process);
}

void AudioEngine::_process_audio_out_node(ModuleProcessor& process)
{
    float* input_buffer = process.audio_input(0);
    assert(process.audio_input_channels(0) == _output_channels);
    assert(_audio_buffer.size() % _output_channels == 0);
    assert(_audio_buffer.size() == process.buffer_frame_count * process.audio_input_channels(0));

    // copy input of the AUDIO_OUT module to the output audio ring buffer
    memcpy(_audio_buffer.data(), input_buffer, process.buffer_frame_count * _output_channels * sizeof(float));
    _audio_ring_buffer.write(_audio_buffer.data(), _audio_buffer.size());
}

void AudioEngine::_s_process_stereo_mixer_node(ModuleProcessor &proc)
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

void AudioEngine::_s_process_message_duplicator_node(ModuleProcessor &proc)
{
    static std::byte buf[MESSAGE_PORT_CAPACITY];

    while (true) {
        unsigned int msg_size = proc.read_message(0, (void*) buf, MESSAGE_PORT_CAPACITY);
        if (msg_size == 0) break;

        for (unsigned int i = 0; i < proc.node.module->output_message_ports.size(); i++) {
            proc.send_message(i, (void*) buf, msg_size);
        }
    }
}

void AudioEngine::_thread_process()
{
    sys::SleepHandle sleep_handle;

    while (_is_engine_runnning)
    {
        std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

        //logger::log_debug("available to write: %lu", _audio_ring_buffer.available_for_write());

        uint64_t ft = _frame_time;
        while (_audio_ring_buffer.available_for_write() >= _frames_per_buffer * _output_channels)
        {
            /*if (_audio_ring_buffer.available_for_write() < _frames_per_buffer * _output_channels)
            {
                break;
            }*/
            const std::lock_guard<std::mutex> _mutex_lock(_mutex);

            // not null if there is an AUDIO_OUT module in the graph
            if (_current_graph != nullptr)
            {
                for (auto &id : _current_graph->process_order) {
                    _process_node(id);
                }
            }

            // there are no AUDIO_OUT modules in the graph... just upload a dummy array (full of 0s)
            else
            {
                std::memset(_audio_buffer.data(), 0, _audio_buffer.size() * sizeof(float));
                _audio_ring_buffer.write(_audio_buffer.data(), _audio_buffer.size());
            }

            ft += _frames_per_buffer;
            _frame_time = ft;
        }

        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        _process_time = (float)std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() / 1000000.0f;

        sleep_handle.sleep(3);
    }
}