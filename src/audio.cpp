#include <iostream>
#include <memory>
#include <cstring>
#include "audio.h"

int AudioDevice::_pa_stream_callback_raw(
    const void* input_buffer,
    void* output_buffer,
    unsigned long frame_count,
    const PaStreamCallbackTimeInfo* time_info,
    PaStreamCallbackFlags status_flags,
    void* userdata
)
{
    AudioDevice* device = (AudioDevice*)userdata;
    return device->_pa_stream_callback(input_buffer, output_buffer, frame_count, time_info, status_flags);
}

static void _pa_panic(PaError err)
{
    printf("PortAudio Error: %s\n", Pa_GetErrorText(err));
    AudioDevice::_pa_stop();
    exit(1);
}

bool AudioDevice::_pa_start()
{
    PaError err = Pa_Initialize();
    if (err != paNoError)
    {
        printf("Could not start PortAudio: %s\n", Pa_GetErrorText(err));
        return true;
    }

    std::cout << "PortAudio available backends:\n";

    int selected_backend = Pa_GetDefaultHostApi();

    for (int i = 0; i < Pa_GetHostApiCount(); i++)
    {
        const PaHostApiInfo* info = Pa_GetHostApiInfo(i);

        if (selected_backend == i)
            std::cout << "  [x] - ";
        else
            std::cout << "  [ ] - ";
        
        std::cout << info->name << "\n";
    }

    return false;
}

void AudioDevice::_pa_stop()
{
    PaError err = Pa_Terminate();
    if (err != paNoError)
        printf("PortAudio error: %s\n", Pa_GetErrorText(err));
}

AudioDevice::AudioDevice(int output_device)
:   ring_buffer(SAMPLE_RATE / 2)
{
    PaError err;

    // if output_device == -1, use default output device
    if (output_device == -1 && (output_device = Pa_GetDefaultOutputDevice()) == paNoDevice)
    {
        printf("no default output device found\n");
        output_device = 0;
    }
    
    PaStreamParameters out_params;
    out_params.channelCount = 2;
    out_params.device = output_device;
    out_params.hostApiSpecificStreamInfo = nullptr;
    out_params.sampleFormat = paFloat32;
    out_params.suggestedLatency = 0.05;
    err = Pa_OpenStream(
        &pa_stream,
        nullptr,
        &out_params, // num output channels (stereo)
        SAMPLE_RATE, // sample rate
        paFramesPerBufferUnspecified, // num frames per buffer (am using own buffer so this is not needed)
        0, // stream flags
        _pa_stream_callback_raw, // callback function
        (void*)this // user data
    );
    if (err != paNoError) _pa_panic(err);

    err = Pa_StartStream(pa_stream);
    if (err != paNoError) _pa_panic(err);
}

AudioDevice::~AudioDevice()
{
    stop();
}

void AudioDevice::stop() {
    if (pa_stream == nullptr) return;

    PaError err;
    err = Pa_StopStream(pa_stream);
    pa_stream = nullptr;
}

void AudioDevice::queue(float* buf, size_t buf_size)
{
    ring_buffer.write(buf, buf_size);
}

size_t AudioDevice::samples_queued() const
{
    return ring_buffer.queued();
}

static inline float clampf(float value, float min, float max)
{
    if (value > max) return max;
    if (value < min) return min;
    return value;
}

int AudioDevice::_pa_stream_callback(
    const void* input_buffer,
    void* output_buffer,
    unsigned long frame_count,
    const PaStreamCallbackTimeInfo* time_info,
    PaStreamCallbackFlags status_flags
) {
    float* out = (float*) output_buffer;
    float sample_count = frame_count * 2;

    // read from the ring buffer and write to out
    // writes zeros for any remaining samples that were not written to
    for (size_t i = ring_buffer.read(out, sample_count); i < sample_count; i++)
    {
        out[i] = 0.0f;
    };

    _frames_written += frame_count;
    return 0;
}

using namespace audiomod;
//////////////////////////////////////
//   NOTE EVENT / MIDI CONVERSION   //
//////////////////////////////////////

size_t MidiMessage::size() const
{
    switch (status) {
        case 0x80: // note off
        case 0x90: // note on
            return sizeof(status) + sizeof(note);
    }

    return 0;
}

size_t NoteEvent::write_midi(MidiMessage* out) const
{
    switch (kind) {
        case NoteOff:
            out->status = 0x80;
            out->note.key = key % 128;
            out->note.velocity = clampf(volume, 0.0f, 1.0f) * 127;
            break;
        
        case NoteOn:
            out->status = 0x90;
            out->note.key = key % 128;
            out->note.velocity = clampf(volume, 0.0f, 1.0f) * 127;
            break;

    }

    return out->size();
}

bool NoteEvent::read_midi(const MidiMessage* in)
{
    switch (in->status & 0xf0) { // the last 4 bits are for the channel number
        case 0x80: // note off
            kind = NoteOff;
            key = in->note.key;
            volume = (float)in->note.velocity / 127;
            return true;
        
        case 0x90: // note on
            kind = NoteOn;
            key = in->note.key;
            volume = (float)in->note.velocity / 127;
            return true;

        default:
            return false;
    }
}



//////////////////////
//   MODULE GRAPH   //
//////////////////////

ModuleNode::ModuleNode(ModuleContext& modctx, std::unique_ptr<ModuleBase>&& mod)
:   modctx(modctx), _module(std::move(mod))
{
    output_array = new float[modctx.frames_per_buffer * modctx.num_channels];
}

ModuleNode::~ModuleNode()
{
    delete[] output_array;
    for (float*& ptr : input_arrays)
        delete[] ptr;
}

void ModuleNode::add_input(ModuleNodeRc module)
{
    input_nodes.push_back(std::move(module));
    input_arrays.push_back(new float[modctx.frames_per_buffer * modctx.num_channels]);
}

bool ModuleNode::remove_input(ModuleNodeRc& module)
{
    for (auto it = input_nodes.begin(); it != input_nodes.end(); it++) {
        if (*it == module) {
            input_nodes.erase(it);
            delete[] input_arrays.back();
            input_arrays.pop_back();
            return false;
        }
    }

    return true;
}

void ModuleNode::connect(ModuleNodeRc& dest)
{
    disconnect();
    dest->add_input(shared_from_this());
    output_node = dest;

    modctx.make_dirty();
}

void ModuleNode::disconnect()
{
    ModuleNodeRc shared = std::enable_shared_from_this<ModuleNode>::shared_from_this();

    if (output_node) output_node->remove_input(shared);
    output_node = nullptr;

    modctx.make_dirty();
}

void ModuleNode::remove_all_connections()
{
    disconnect();

    for (ModuleNodeRc& input : input_nodes)
        input->disconnect();
}




ModuleContext::ModuleContext(int sample_rate, int num_channels, size_t buffer_size)
:   sample_rate(sample_rate), num_channels(num_channels), frames_per_buffer(buffer_size)
{
    audio_buffer = new float[frames_per_buffer * num_channels];

    // fill dummy buffer with zeroes
    memset(dummy_buffer, 0, sizeof(dummy_buffer));
}

ModuleContext::~ModuleContext()
{
    delete[] audio_buffer;
}

void ModuleContext::make_dirty()
{}

void ModuleContext::process_node(ModuleNode& node)
{
    // get data in inputs
    for (ModuleNodeRc& input : node.input_nodes)
    {
        process_node(node);
    }

    node.module().process(
        node.input_arrays.data(),
        node.output_array,
        node.input_nodes.size(),
        frames_per_buffer * num_channels,
        sample_rate,
        num_channels
    );
}

size_t ModuleContext::process(float* &buffer)
{
    for (ModuleNodeRc& input_node : _dest->input_nodes)
    {
        process_node(*input_node.get());
    }

    // combine all inputs into one buffer
    for (size_t i = 0; i < frames_per_buffer * num_channels; i += 2) {
        audio_buffer[i] = 0.0f;
        audio_buffer[i + 1] = 0.0f;

        for (const float* input_array : _dest->input_arrays) {
            audio_buffer[i] += input_array[i];
            audio_buffer[i + 1] += input_array[i + 1];
        }
    }

    buffer = audio_buffer;
    return frames_per_buffer * num_channels;
}