#include <algorithm>
#include <chrono>
#include <climits>
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
#include "audio_renderer.hpp"
#include "../log.hpp"

using namespace modules;

const char* AudioEngine::MODULE_CLASS_AUDIO_OUT = "AUDIO_OUT";
const char* AudioEngine::MODULE_CLASS_STEREO_MIXER = "STEREO_MIXER";
const char* AudioEngine::MODULE_CLASS_MESSAGE_DUPLICATOR = "MESSAGE_DUPLICATOR";

static ModuleInfo AUDIO_OUT_INFO { AudioEngine::MODULE_CLASS_AUDIO_OUT, "Audio Output", "", 0, -1, -1, -1 };
static ModuleInfo STEREO_MIXER_INFO { AudioEngine::MODULE_CLASS_STEREO_MIXER, "Stereo Mixer", "", 0, 0, -1, -1 };
static ModuleInfo MESSAGE_DUPLICATOR_INFO { AudioEngine::MODULE_CLASS_MESSAGE_DUPLICATOR, "Message Duplicator", "", -1, -1, 0, 0 };

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
    _frames_per_buffer(512)
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

    _output_channels = out_params.channelCount;
    assert(_output_channels == 2);

    renderer = std::make_unique<AudioRenderer>(*this);

    err = Pa_StartStream(_pa_stream);
    if (err != paNoError) pa_panic(err);
    _is_engine_runnning = true;
}

AudioEngine::~AudioEngine()
{
    _is_engine_runnning = false;
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

    PaTime start = Pa_GetStreamTime(self->_pa_stream);
    assert(frame_count == self->frames_per_buffer());
    self->renderer->render((float*) output_buffer);
    PaTime end = Pa_GetStreamTime(self->_pa_stream);

    self->_process_time = end - start;
    //logger::log_debug("available to read: %lu", self->_audio_ring_buffer.available_for_read());

    /*float* out_samples = (float*) output_buffer;
    if (!self->_audio_ring_buffer.read(out_samples, frame_count * self->_output_channels))
    {
        // #ifdef DEBUG
        // logger::log_debug("AudioEngine::_pa_stream_callback: not enough audio data to fill buffer");
        // #endif
        memset(out_samples, 0, frame_count * self->_output_channels * sizeof(float));
    }*/

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
    std::shared_ptr<ModuleData::ModuleInstance> instance = std::make_shared<ModuleData::ModuleInstance>();
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
            instance->input_audio_ports[i] = ModuleData::ModuleAudioPort(
                (uint8_t) _output_channels,
                0,
                0
            );
        }

        instance->userdata = renderer.get();
        instance->processor = AudioRenderer::process_audio_out_node;
    }
    else if (mod_class == MODULE_CLASS_STEREO_MIXER)
    {
        instance->name = "Stereo Mixer";
        instance->output_audio_ports.resize(1);
        instance->output_audio_ports[0] = ModuleData::ModuleAudioPort(
            2,
            0,
            0
        );

        instance->is_stereo_mixer = true;
        instance->processor = AudioRenderer::process_stereo_mixer_node;
    }
    else if (mod_class == MODULE_CLASS_MESSAGE_DUPLICATOR)
    {
        instance->name = "Message Duplicator";
        instance->input_message_ports.resize(1);
        instance->input_message_ports[0] = ModuleData::ModuleMessagePort(
            0,
            0
        );

        instance->is_message_duplicator = true;
        instance->processor = AudioRenderer::process_message_duplicator_node;
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

    std::shared_ptr<ModuleData::ModuleInstance>& mod = it->second;

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

const ModuleInfo *AudioEngine::get_module_info(const std::string &modclass) {
    if (modclass == MODULE_CLASS_AUDIO_OUT) return &AUDIO_OUT_INFO;
    if (modclass == MODULE_CLASS_STEREO_MIXER) return &STEREO_MIXER_INFO;
    if (modclass == MODULE_CLASS_MESSAGE_DUPLICATOR) return &MESSAGE_DUPLICATOR_INFO;
    
    for (const auto &info : _available_module_classes) {
        if (info.class_name == modclass) {
            return &info;
        }
    }

    return nullptr;
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
    
    const ModuleData::ModuleInstance &mod = *it->second;
    return mod.class_name;
}

const std::string& AudioEngine::module_name(ModuleID mod_id) const
{
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end())
        return EMPTY_STRING;
    
    const ModuleData::ModuleInstance &mod = *it->second;
    return mod.name;
}







///////////////////////////////
// MODULE AUDIO INPUT/OUTPUT //
///////////////////////////////
#pragma region AUDIO IN/OUT

unsigned int AudioEngine::audio_input_count(ModuleID mod_id) const
{
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end())
        return 0;
    
    const ModuleData::ModuleInstance &mod = *it->second;
    return mod.input_audio_ports.size();
}

unsigned int AudioEngine::audio_output_count(ModuleID mod_id) const
{
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end())
        return 0;
    
    const ModuleData::ModuleInstance &mod = *it->second;
    return mod.output_audio_ports.size();
}

std::uint8_t AudioEngine::audio_input_channel_count(ModuleID mod_id, unsigned int index) const
{
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end())
        return 0;

    const ModuleData::ModuleInstance &mod = *it->second;

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

    const ModuleData::ModuleInstance &mod = *it->second;
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

    const ModuleData::ModuleInstance &mod = *it->second;
    
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

    const ModuleData::ModuleInstance &mod = *it->second;
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
    // logger::log_info(
    //     "connect audio %s output %i to %s input %i",
    //     module_name(mod_a_id).c_str(),
    //     out_index,
    //     module_name(mod_b_id).c_str(),
    //     in_index
    // );

    const auto &it_a = _modules.find(mod_a_id);
    if (it_a == _modules.end()) return false;

    const auto &it_b = _modules.find(mod_b_id);
    if (it_b == _modules.end()) return false;

    ModuleData::ModuleInstance &mod_a = *it_a->second;
    ModuleData::ModuleInstance &mod_b = *it_b->second;

    if (out_index >= mod_a.output_audio_ports.size()) return false;

    if (mod_b.is_stereo_mixer)
    {
        if (mod_a.output_audio_ports[out_index].channel_count != 2) return false;
        disconnect_audio_output(mod_a_id, out_index);

        mod_a.output_audio_ports[out_index].connected_module = mod_b_id;
        mod_a.output_audio_ports[out_index].connection_port = 0;
        mod_a.output_audio_ports[out_index].modulator = false;

        mod_b.input_audio_ports.push_back(ModuleData::ModuleAudioPort(
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
        mod_a.output_audio_ports[out_index].modulator = true;

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

    ModuleData::ModuleInstance &mod = *it->second;
    if (out_index >= mod.output_audio_ports.size()) return false;

    if (mod.output_audio_ports[out_index].connected_module != 0)
    {
        _is_graph_dirty = true;
        ModuleData::ModuleInstance &connected = *_modules.at(mod.output_audio_ports[out_index].connected_module);

        if (mod.output_audio_ports[out_index].modulator) {
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
        } else {
            unsigned int mod_index = mod.output_audio_ports[out_index].connection_port;
            connected.modulators[mod_index].control.connected_module = 0;
            connected.modulators[mod_index].control.connection_port = 0;
        }
    }

    mod.output_audio_ports[out_index].connection_port = 0;
    mod.output_audio_ports[out_index].connected_module = 0;
    mod.output_audio_ports[out_index].modulator = false;

    return true;
}

bool AudioEngine::disconnect_audio_input(ModuleID mod_id, unsigned int in_index)
{
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end()) return false;

    ModuleData::ModuleInstance &mod = *it->second;

    if (mod.is_stereo_mixer)
    {
        if (in_index > 0) return false;

        for (auto &cn : mod.input_audio_ports)
        {
            _is_graph_dirty = true;
            ModuleData::ModuleInstance &connected = *_modules.at(cn.connected_module);
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
            ModuleData::ModuleInstance &connected = *_modules.at(mod.input_audio_ports[in_index].connected_module);
            unsigned int out_index = mod.input_audio_ports[in_index].connection_port;
            connected.output_audio_ports[out_index].connected_module = 0;
            connected.output_audio_ports[out_index].connection_port = 0;
            connected.output_audio_ports[out_index].modulator = false;
        }

        mod.input_audio_ports[in_index].connection_port = 0;
        mod.input_audio_ports[in_index].connected_module = 0;
    }

    return true;
}
#pragma endregion AUDIO IN/OUT






//////////////////////////////////////
// MODULE MESSAGE PORT INPUT/OUTPUT //
//////////////////////////////////////
#pragma region MESSAGE IN/OUT

unsigned int AudioEngine::message_input_count(ModuleID mod_id) const
{
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end())
        return 0;
    
    const ModuleData::ModuleInstance &mod = *it->second;
    return mod.input_message_ports.size();
}

unsigned int AudioEngine::message_output_count(ModuleID mod_id) const
{
    const auto& it = _modules.find(mod_id);
    if (it == _modules.end())
        return 0;

    const ModuleData::ModuleInstance &mod = *it->second;
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

    const ModuleData::ModuleInstance &mod = *it->second;
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

    const ModuleData::ModuleInstance &mod = *it->second;
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
    // logger::log_info(
    //     "connect msg %s output %i to %s input %i",
    //     module_name(mod_a_id).c_str(),
    //     out_index,
    //     module_name(mod_b_id).c_str(),
    //     in_index
    // );

    const auto &it_a = _modules.find(mod_a_id);
    if (it_a == _modules.end()) return false;

    const auto &it_b = _modules.find(mod_b_id);
    if (it_b == _modules.end()) return false;

    ModuleData::ModuleInstance &mod_a = *it_a->second;
    ModuleData::ModuleInstance &mod_b = *it_b->second;

    if (in_index >= mod_b.input_message_ports.size()) return false;

    if (mod_a.is_message_duplicator) {
        if (out_index > 0) return false;

        disconnect_message_input(mod_b_id, in_index);

        mod_a.output_message_ports.push_back(ModuleData::ModuleMessagePort(
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

    ModuleData::ModuleInstance &mod = *it->second;

    if (mod.is_message_duplicator) {
        if (out_index > 0) return false;

        for (auto &port : mod.output_message_ports) {
            if (port.connected_module == 0) continue;
            _is_graph_dirty = true;

            ModuleData::ModuleInstance &connected = *_modules.at(port.connected_module);
            connected.input_message_ports[port.connection_port].connected_module = 0;
            connected.input_message_ports[port.connection_port].connection_port = 0;
        }

        mod.output_message_ports.clear();
    } else {
        if (out_index >= mod.output_message_ports.size()) return false;

        if (mod.output_message_ports[out_index].connected_module != 0)
        {
            _is_graph_dirty = true;
            ModuleData::ModuleInstance &connected = *_modules.at(mod.output_message_ports[out_index].connected_module);
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

    ModuleData::ModuleInstance &mod = *it->second;
    if (in_index >= mod.input_message_ports.size()) return false;

    if (mod.input_message_ports[in_index].connected_module != 0)
    {
        ModuleData::ModuleInstance &connected = *_modules.at(mod.input_message_ports[in_index].connected_module);

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
#pragma endregion MESSAGE IN/OUT

















//////////////
// CONTROLS //
//////////////
#pragma region CONTROLS

unsigned int AudioEngine::control_count(ModuleID mod_id) const
{
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end()) return false;
    const ModuleData::ModuleInstance &mod = *it->second;

    return mod.controls.size();
}

ModuleControlDataType AudioEngine::control_data_type(ModuleID mod_id, unsigned int index) const
{
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end()) return ModuleControlDataType::UNKNOWN;
    const ModuleData::ModuleInstance &mod = *it->second;

    if (index >= mod.controls.size()) return ModuleControlDataType::UNKNOWN;
    return mod.controls[index].data_type;
}

const std::string AudioEngine::control_name(ModuleID mod_id, unsigned int index) const
{
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end()) return "";
    const ModuleData::ModuleInstance &mod = *it->second;
    if (index >= mod.controls.size()) return "";

    return mod.controls[index].name;
}

bool AudioEngine::control_get_index(ModuleID mod_id, const std::string &name, unsigned int &index) const {
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end()) return false;
    const ModuleData::ModuleInstance &mod = *it->second;

    index = 0;
    for (auto it = mod.controls.begin(); it != mod.controls.end(); it++) {
        if (it->name == name) {
            return true;
        }
        index++;
    }

    return false;
}

#pragma endregion CONTROLS







///////////////////////////
// MODULATOR CONNECTIONS //
///////////////////////////
#pragma region MODULATORS

bool AudioEngine::create_modulator(ModuleID mod_id, unsigned int &out_mod_index) {
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end()) return false;
    std::shared_ptr<ModuleData::ModuleInstance>& mod = it->second;

    ModuleData::Modulator modu {};
    modu.control = ModuleData::ModuleAudioPort { 1, 0, 0 };
    mod->modulators.push_back(modu);

    out_mod_index = mod->modulators.size() - 1;
    return true;
}

void AudioEngine::destroy_modulator(ModuleID mod_id, unsigned int mod_index) {
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end()) return;
    std::shared_ptr<ModuleData::ModuleInstance>& mod = it->second;

    if (mod_index >= mod->modulators.size()) return;
    mod->modulators.erase(mod->modulators.begin() + mod_index);
}

unsigned int AudioEngine::modulator_count(ModuleID mod_id) const {
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end()) return 0;
    const std::shared_ptr<ModuleData::ModuleInstance>& mod = it->second;

    return mod->modulators.size();
}

bool AudioEngine::connect_modulator(ModuleID mod_a_id, ModuleID mod_b_id, unsigned int out_index, unsigned int mod_index) {
    const auto &it_a = _modules.find(mod_a_id);
    if (it_a == _modules.end()) return false;

    const auto &it_b = _modules.find(mod_b_id);
    if (it_b == _modules.end()) return false;

    ModuleData::ModuleInstance &mod_a = *it_a->second;
    ModuleData::ModuleInstance &mod_b = *it_b->second;

    if (out_index >= mod_a.output_audio_ports.size()) return false;

    if (mod_index >= mod_b.modulators.size()) return false;
    if (mod_a.output_audio_ports[out_index].channel_count != 1) return false;

    disconnect_audio_output(mod_a_id, out_index);
    disconnect_modulator_input(mod_b_id, mod_index);

    mod_a.output_audio_ports[out_index].connected_module = mod_b_id;
    mod_a.output_audio_ports[out_index].connection_port = mod_index;
    mod_a.output_audio_ports[out_index].modulator = true;

    mod_b.modulators[mod_index].control.connected_module = mod_a_id;
    mod_b.modulators[mod_index].control.connection_port = out_index;

    _is_graph_dirty = true;
    return true;
}


bool AudioEngine::disconnect_modulator_input(ModuleID mod_id, unsigned int mod_index) {
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end()) return false;
    ModuleData::ModuleInstance &mod = *it->second;
    
    if (mod_index >= mod.modulators.size()) return false;

    if (mod.modulators[mod_index].control.connected_module != 0)
    {
        _is_graph_dirty = true;
        ModuleData::ModuleInstance &connected = *_modules.at(mod.modulators[mod_index].control.connected_module);
        unsigned int out_index = mod.modulators[mod_index].control.connection_port;
        connected.output_audio_ports[out_index].connected_module = 0;
        connected.output_audio_ports[out_index].connection_port = 0;
        connected.output_audio_ports[out_index].modulator = false;
    }

    mod.modulators[mod_index].control.connection_port = 0;
    mod.modulators[mod_index].control.connected_module = 0;

    return true;
}

ModuleData::ModulatorControl* AudioEngine::modulator_find_control(ModuleData::Modulator &modu, unsigned int ctl) {
    ModuleData::ModulatorControl *ctl_mod = nullptr;
    for (auto &target : modu.targets) {
        if (target.control_index == ctl) {
            return &target;
        }
    }

    // if it doesn't exist, create a new target control
    modu.targets.push_back(ModuleData::ModulatorControl {});
    ctl_mod = &modu.targets.back();
    ctl_mod->control_index = ctl;

    return ctl_mod;
}

bool AudioEngine::modulator_untarget(ModuleID mod_id, unsigned int modu_idx, unsigned int ctl) {
    const auto &it = _modules.find(mod_id);
    if (it == _modules.end()) return false;
    ModuleData::ModuleInstance &mod = *it->second;

    if (modu_idx >= mod.modulators.size()) return false; // modu existence check
    auto &modu = mod.modulators[modu_idx];
    
    for (auto it = modu.targets.begin(); it != modu.targets.end(); it++) {
        if (it->control_index == ctl) {
            modu.targets.erase(it);
            return true;
        }
    }

    return false;
}

#pragma endregion MODULATORS













////////////////
// PROCESSING //
////////////////
void AudioEngine::update()
{
    // call module idle processes
    for (auto& [ id, inst ] : _modules)
    {
        if (inst->idle == nullptr) continue;
        inst->idle(*this, id, inst->userdata);
    }
    
    // make new audio graph
    if (_is_graph_dirty) {
        AudioRenderer::InMessage msg;
        msg.kind = AudioRenderer::MESSAGE_NEW_GRAPH;
        msg.graph = renderer->build_graph();
        renderer->send_message(msg);
        _is_graph_dirty = false;
    }

    // read out messages
    AudioRenderer::OutMessage out_msg;
    bool graph_did_update = false;

    while (renderer->get_message(out_msg)) {
        switch (out_msg.kind) {
            case AudioRenderer::MESSAGE_GRAPH_UPDATED:
                graph_did_update = true;
                break;
        }
    }

    // flush destroy queue when graph is updated
    if (graph_did_update) {
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
    }
}