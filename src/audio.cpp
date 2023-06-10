#include <cassert>
#include <exception>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <iostream>
#include "audio.h"
#include "modules/modules.h"
#include <portaudio.h>
#include "sys.h"
#include <imgui.h>

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

    return false;
}

void AudioDevice::_pa_stop()
{
    PaError err = Pa_Terminate();
    if (err != paNoError)
        printf("PortAudio error: %s\n", Pa_GetErrorText(err));
}

AudioDevice::AudioDevice(int output_device) {
    // NOTE: PaTime Pa_GetStreamTime(PaStream* stream)
    
    // if output_device == -1, use default output device
    PaError err;

    // TODO: ability to use devices other than default
    err = Pa_OpenDefaultStream(
        &pa_stream,
        0, // num input channels
        2, // num output channels (stereo)
        paFloat32, // 32-bit float output
        SAMPLE_RATE, // sample rate
        paFramesPerBufferUnspecified, // num frames per buffer (am using own buffer so this is not needed)
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

    if (_thread_audio_buffer != nullptr)
        delete[] _thread_audio_buffer;

    if (_new_audio_buffer != nullptr)
        delete[] _new_audio_buffer;

    if (_old_audio_buffer != nullptr)
        delete[] _old_audio_buffer;
}

int AudioDevice::_pa_stream_callback(
    const void* input_buffer,
    void* output_buffer,
    unsigned long frame_count,
    const PaStreamCallbackTimeInfo* time_info,
    PaStreamCallbackFlags status_flags
)
{
    // fetch new buffer if it exists
    // ...buffer size changes happen so infrequently
    // may as well use spinlocks?
    {
        float* new_buf = _new_audio_buffer;
        if (new_buf != nullptr)
        {
            _thread_buffer_size = buffer_size;
            _new_audio_buffer = nullptr;
            _old_audio_buffer = _thread_audio_buffer;
            _thread_audio_buffer = new_buf;
        }
    }

    constexpr double M_PI = 3.14159265358979323846;
    static double phase = 0.0f;

    float* out = (float*)output_buffer;
    unsigned int i;
    (void)input_buffer; // prevent unused variable warning

    for (i = 0; i < frame_count; i++)
    {
        float val = sin(phase) * 0.1f;
        *out++ = val;
        *out++ = val;

        phase += 440.0 * (2.0 * M_PI) / sample_rate();
        if (phase > 2.0 * M_PI) phase -= 2.0 * M_PI;
    }

    double dt = (double)frame_count / sample_rate();
    _time = _time + dt;

    return 0;
}

void AudioDevice::update()
{
    // delete old audio thread buffer if needed
    float* old_buf = _old_audio_buffer;
    if (old_buf != nullptr)
    {
        _old_audio_buffer = nullptr;
        delete[] old_buf;
    }

    // set new audio thread buffer if resized
    if (_old_buffer_size != buffer_size)
    {
        _old_buffer_size = buffer_size;
        _new_audio_buffer = new float[buffer_size * num_channels()];
    }
}




// AUDIO MODULES //
using namespace audiomod;

ModuleOutputTarget::ModuleOutputTarget() {};

ModuleOutputTarget::~ModuleOutputTarget() {
    for (ModuleBase* mod : _inputs) {
        mod->disconnect();
    }
}

void ModuleOutputTarget::add_input(ModuleBase* module) {
    _inputs.push_back(module);
    _input_arrays.push_back(nullptr);
}

bool ModuleOutputTarget::remove_input(ModuleBase* module) {
    for (auto it = _inputs.begin(); it != _inputs.end(); it++) {
        if (*it == module) {
            _inputs.erase(it);
            _input_arrays.pop_back();
            return false;
        }
    }

    return true;
}


//////////////////////
//   MODULE BASE    //
//////////////////////

// TODO: remove audio buffers in ModuleBase, as
// destination module is the one that now
// creates and owns the audio buffers

ModuleBase::ModuleBase(bool has_interface) :
    _output(nullptr),
    _audio_buffer(nullptr),
    _audio_buffer_size(0),
    show_interface(false),
    _has_interface(has_interface),
    id(""),
    name("Module")
{};

ModuleBase::~ModuleBase() {
    remove_all_connections();

    if (_audio_buffer != nullptr) delete[] _audio_buffer;
}

void ModuleBase::connect(ModuleOutputTarget* dest) {
    disconnect();
    dest->add_input(this);
    _output = dest;

    if (_dest) _dest->make_dirty();
}

void ModuleBase::disconnect() {
    if (_output != nullptr) _output->remove_input(this);
    _output = nullptr;

    if (_dest) _dest->make_dirty();
}

void ModuleBase::remove_all_connections() {
    disconnect();

    for (ModuleBase* input : _inputs) {
        input->disconnect();
    }
}

// allocate buffers, not called by audio thread
void ModuleBase::prepare_audio(DestinationModule* dest)
{
    if (_dest != dest)
        dest->make_dirty();

    _dest = dest;

    // audio buffer array to correct size
    size_t desired_size = dest->buffer_size * dest->channel_count;
    if (_audio_buffer == nullptr || _audio_buffer_size != desired_size) {
        if (_audio_buffer != nullptr) delete[] _audio_buffer;
        _audio_buffer_size = desired_size;
        _audio_buffer = new float[_audio_buffer_size];
    }

    for (ModuleBase* input : _inputs)
        input->prepare_audio(dest);
}

float* ModuleBase::get_audio() {
    // accumulate inputs
    for (size_t i = 0; i < _inputs.size(); i++) {
        _input_arrays[i] = _inputs[i]->get_audio();
    }

    // processing
    size_t num_inputs = _inputs.size();
    process(
        num_inputs > 0 ? &_input_arrays.front() : nullptr, // float input[][]
        _audio_buffer, // float output[]
        num_inputs, // num inputs
        _dest->buffer_size * _dest->channel_count, // number of samples
        _dest->sample_rate, // sample rate
        _dest->channel_count // channel count
    );

    return _audio_buffer;
}

bool ModuleBase::has_interface() const {
    return _has_interface;
}

bool ModuleBase::render_interface() {
    if (!show_interface) return false;

    char window_name[128];
    snprintf(window_name, 128, "%s - %s###%p", name.c_str(), parent_name == nullptr ? "" : parent_name, this);

    if (ImGui::Begin(window_name, &show_interface, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize)) {
        _interface_proc();
    } ImGui::End();

    return show_interface;
}










//////////////////////////
//  DESTINATION MODULE  //
//////////////////////////

DestinationModule::DestinationModule(int sample_rate, int num_channels, size_t buffer_size) : time(0.0), sample_rate(sample_rate), channel_count(num_channels), buffer_size(buffer_size) {
    _audio_buffer = nullptr;
    _prev_buffer_size = 0;
}

DestinationModule::~DestinationModule() {
    if (_audio_buffer != nullptr) delete[] _audio_buffer;
}

// allocate buffers, not called by audio thread
void DestinationModule::prepare()
{
    if (_audio_buffer == nullptr || _prev_buffer_size != buffer_size * channel_count) {
        if (_audio_buffer != nullptr) delete[] _audio_buffer;
        _prev_buffer_size = buffer_size * channel_count;
        _audio_buffer = new float[_prev_buffer_size];
    }

    // free old graph set from audio thread
    ModuleNode* _old_graph = old_graph;
    if (_old_graph != nullptr)
    {
        std::cout << "free\n";
        free_graph(_old_graph);
        old_graph = nullptr;
    }

    // update buffers of modules connected to this destination
    for (ModuleBase* input : _inputs)
        input->prepare_audio(this);

    // if dirty, that means audio graph has changed and needs
    // reconstruction for the audio thread
    if (is_dirty)
    {
        std::cout << "dirty\n";
        new_graph = construct_graph();
        is_dirty = false;
    }
}

DestinationModule::ModuleNode* DestinationModule::_create_node(ModuleBase* module)
{
    ModuleNode* node = new ModuleNode;

    node->module = module;
    node->output = new float[buffer_size * channel_count];
    node->buf_size = buffer_size;
    
    // get inputs
    node->num_inputs = module->get_inputs().size();
    node->inputs = new ModuleNode*[node->num_inputs];
    size_t i = 0;
    for (ModuleBase* input_mod : module->get_inputs())
    {
        node->inputs[i++] = _create_node(input_mod);
    }

    // accumulate input arrays
    node->input_arrays = new float* [node->num_inputs];
    for (i = 0; i < node->num_inputs; i++)
        node->input_arrays[i] = node->inputs[i]->output;

    return node;
}

// same as _create_node, but module is assigned to nullptr and
// it reads from DestinationModule (a ModuleOutputTarget)
DestinationModule::ModuleNode* DestinationModule::construct_graph()
{
    ModuleNode* node = new ModuleNode;

    node->module = nullptr;
    node->output = new float[buffer_size * channel_count];
    node->buf_size = buffer_size;

    // get inputs
    node->num_inputs = get_inputs().size();
    node->inputs = new ModuleNode * [node->num_inputs];
    size_t i = 0;
    for (ModuleBase* input_mod : get_inputs())
    {
        node->inputs[i++] = _create_node(input_mod);
    }

    // accumulate input arrays
    node->input_arrays = new float* [node->num_inputs];
    for (i = 0; i < node->num_inputs; i++)
        node->input_arrays[i] = node->inputs[i]->output;

    return node;
}

void DestinationModule::free_graph(ModuleNode* node)
{
    for (size_t i = 0; i < node->num_inputs; i++)
    {
        node->input_arrays[i] = nullptr;
        free_graph(node->inputs[i]);
        node->inputs[i] = nullptr;
    }

    delete[] node->input_arrays;
    delete[] node->inputs;
    delete[] node->output;
    delete node;
}

size_t DestinationModule::process(float** output) {
    // obtain the updated graph if needed
    ModuleNode* _new_graph = new_graph;
    if (_new_graph != nullptr) {
        old_graph = _thread_audio_graph;
        _thread_audio_graph = _new_graph;
        new_graph = nullptr;
    }

    process_node(_thread_audio_graph);

    *output = _thread_audio_graph->output;
    return buffer_size * channel_count;
}

void DestinationModule::process_node(ModuleNode* node)
{
    // get data in inputs
    for (size_t i = 0; i < node->num_inputs; i++)
    {
        process_node(node->inputs[i]);
    }

    if (node->module)
        node->module->process(node->input_arrays, node->output, node->num_inputs, node->buf_size, sample_rate, channel_count);
    
    else // if this is the root module, then combine all inputs into one buffer
    {
        for (size_t i = 0; i < node->buf_size * channel_count; i += 2) {
            node->output[i] = 0.0f;
            node->output[i + 1] = 0.0f;

            for (size_t j = 0; j < node->num_inputs; j++) {
                node->output[i] += node->input_arrays[j][i];
                node->output[i + 1] += node->input_arrays[j][i + 1];
            }
        }
    }
}








//////////////////////
//   EFFECTS RACK   //
//////////////////////
EffectsRack::EffectsRack() : output(nullptr)
{}

EffectsRack::~EffectsRack() {
    for (ModuleBase* input : inputs)
        input->disconnect();
    
    if (!modules.empty()) modules.back()->disconnect();

    for (ModuleBase* module : modules) {
        delete module;
    }
}

void EffectsRack::insert(ModuleBase* module) {
    if (modules.empty()) return insert(module, 0);
    return insert(module, modules.size());
}

void EffectsRack::insert(ModuleBase* module, size_t position) {
    // if there is nothing in the effects rack, this is a simple operation
    if (modules.empty()) {
        for (ModuleBase* input : inputs)
        {
            input->disconnect();
            input->connect(module);
        }
        
        if (output != nullptr) {
            module->connect(output);
        } 

    // insertion at beginning
    } else if (position == 0) {
        for (ModuleBase* input : inputs)
        {
            input->disconnect();
            input->connect(module);
        }
        
        module->connect(modules[1]);

    // insertion at end
    } else if (position == modules.size()) {
        modules.back()->disconnect();
        modules.back()->connect(module);
        if (output != nullptr) module->connect(output);
        
    // insertion at middle
    } else {
        ModuleBase* prev = modules[position - 1];
        ModuleBase* next = modules[position];
        
        prev->disconnect();
        prev->connect(module);

        module->connect(next);
    }

    // now, add module to the array
    modules.insert(modules.begin() + position, module);
}

ModuleBase* EffectsRack::remove(size_t position) {
    if (modules.empty()) return nullptr;

    ModuleBase* target_module = modules[position];
    target_module->remove_all_connections();

    // if there is only one item in the rack
    if (modules.size() == 1) {
        for (ModuleBase* input : inputs)
            input->connect(output);
        
    // remove the first module
    } else if (position == 0) {
        for (ModuleBase* input : inputs)
            input->connect(modules[1]);
        
    // remove the last module
    } else if (position == modules.size() - 1) {
        if (output != nullptr) modules[modules.size() - 2]->connect(output);
    
    // remove a module in the middle
    } else {
        ModuleBase* prev = modules[position - 1];
        ModuleBase* next = modules[position + 1];

        prev->connect(next);
    }

    // now, remove the module in the array
    modules.erase(modules.begin() + position);

    // return the pointer to the module so caller can potentially free it
    return target_module;
}

bool EffectsRack::disconnect_input(ModuleBase* input) {
    for (auto it = inputs.begin(); it != inputs.end(); it++)
    {
        if (*it == input)
        {
            inputs.erase(it);
            return true;
        }
    }

    return false;
}

ModuleOutputTarget* EffectsRack::disconnect_output()
{
    ModuleOutputTarget* old_output = output;

    if (output != nullptr)
    {
        if (modules.empty())
        {
            for (ModuleBase* input : inputs)
                input->disconnect();
        }
        else modules.back()->disconnect();
    }

    output = nullptr;
    return old_output;
}

void EffectsRack::connect_input(ModuleBase* new_input) {
    if (modules.empty()) {
        if (output != nullptr)   
            new_input->connect(output);
    } else {
        new_input->connect(modules.front());
    }
    
    inputs.push_back(new_input);
}

ModuleOutputTarget* EffectsRack::connect_output(ModuleOutputTarget* new_output) {
    ModuleOutputTarget* old_output = disconnect_output();
    output = new_output;

    if (output != nullptr) {
        if (modules.empty()) {
            for (ModuleBase* input : inputs)
                input->connect(output);
            
        } else {
            modules.back()->connect(output);
        }
    }
    
    return old_output;
}

void EffectsRack::disconnect_all_inputs()
{
    for (ModuleBase* input : inputs)
    {
        input->disconnect();
    }

    inputs.clear();
}















//////////////////////
//   EFFECTS BUS    //
//////////////////////
FXBus::FXBus()
{
    strcpy(name, "FX Bus");
    rack.connect_output(&controller);
}

ModuleOutputTarget* FXBus::connect_output(ModuleOutputTarget* output)
{
    ModuleOutputTarget* old_output = controller.get_output();
    controller.disconnect();
    controller.connect(output);
    return old_output; 
}

ModuleOutputTarget* FXBus::disconnect_output()
{
    ModuleOutputTarget* old_output = controller.get_output();
    controller.disconnect();
    return old_output;
}

FXBus::ControllerModule::ControllerModule() : ModuleBase(false) { }

void FXBus::ControllerModule::process(
    float** inputs,
    float* output,
    size_t num_inputs,
    size_t buffer_size,
    int sample_rate,
    int channel_count
)
{
    float smp[2];
    float accum[2];
    bool is_muted = mute || mute_override;

    float factor = powf(10.0f, gain / 10.0f);

    // clear buffer to zero
    for (size_t i = 0; i < buffer_size; i++)
    {
        output[i] = 0.0f;
    }

    for (size_t i = 0; i < buffer_size; i += 2)
    {
        smp[0] = 0.0f;
        smp[1] = 0.0f;

        for (size_t j = 0; j < num_inputs; j++)
        {
            smp[0] += inputs[j][i] * factor;
            smp[1] += inputs[j][i + 1] * factor;
        }

        if (!is_muted)
        {
            output[i] = smp[0];
            output[i + 1] = smp[1];
        }
        
        if (fabsf(smp[0]) > smp_accum[0]) smp_accum[0] = fabsf(smp[0]);
        if (fabsf(smp[1]) > smp_accum[1]) smp_accum[1] = fabsf(smp[1]);

        if (++smp_count > window_size)
        {
            analysis_volume[0] = smp_accum[0];
            analysis_volume[1] = smp_accum[1];

            smp_accum[0] = smp_accum[1] = 0.0f;
            smp_count = 0;
        }
    }
}