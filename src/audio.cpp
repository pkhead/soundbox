#include <cassert>
#include <exception>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <iostream>
#include "audio.h"
#include "modules/modules.h"
#include "soundio.h"
#include "sys.h"
#include <imgui.h>

void AudioDevice::soundio_write_callback(SoundIoOutStream* outstream, int frame_count_min, int frame_count_max) {
    AudioDevice* self = (AudioDevice*)outstream->userdata;

    // reallocate buffer if change was detected
    if (self->buffer_size_change.change)
    {
        self->buffer_size_change.change = false;
        self->buffer_size = self->buffer_size_change.new_size * self->num_channels();

        if (self->audio_buffer != nullptr) delete[] self->audio_buffer;
        self->audio_buffer = new float[self->buffer_size];
        self->buf_pos = self->buffer_size;
    }

    SoundIoChannelArea* areas;
    int frame_count, err;
    int frames_left = frame_count_max;
    float* buffer; // data from write_callback
    
    // if a write callback was provided
    if (self->write_callback)
    {
        while (frames_left > 0)
        {
            frame_count = frames_left;
            if ((err = soundio_outstream_begin_write(outstream, &areas, &frame_count)))
            {
                fprintf(stderr, "begin write error: %s\n", soundio_strerror(err));
                return;
            }
            if (frame_count <= 0) break;

            for (int frame = 0; frame < frame_count; frame++)
            {
                for (int ch = 0; ch < outstream->layout.channel_count; ch++)
                {
                    // if ran out of data in buffer, compute new data
                    if (self->buf_pos >= self->buffer_size)
                    {
                        size_t b_size = self->write_callback(self, &buffer);
                        assert(b_size == self->buffer_size);
                        memcpy(self->audio_buffer, buffer, self->buffer_size * sizeof(float));
                        self->buf_pos = 0;
                    }

                    // write this sample
                    memcpy(areas[ch].ptr, &self->audio_buffer[self->buf_pos], outstream->bytes_per_sample);
                    areas[ch].ptr += areas[ch].step;
                    self->buf_pos++;
                }
            }

            if ((err = soundio_outstream_end_write(outstream))) {
                fprintf(stderr, "end write error: %s\n", soundio_strerror(err));
                return;
            }

            frames_left -= frame_count;
        }
    }
    else
    {
        // there is no write callback, fill with zeroes
        printf("no write callback!\n");

        while (frames_left > 0) {
            frame_count = frames_left;
            if ((err = soundio_outstream_begin_write(outstream, &areas, &frame_count))) {
                fprintf(stderr, "begin write error: %s\n", soundio_strerror(err));
                return;
            }
            if (frame_count <= 0) break;

            for (int frame = 0; frame < frame_count; frame++) {
                for (int ch = 0; ch < outstream->layout.channel_count; ch++) {
                    memset(areas[ch].ptr, 0, outstream->bytes_per_sample);
                    areas[ch].ptr += areas[ch].step;
                }
            }

            if ((err = soundio_outstream_end_write(outstream))) {
                fprintf(stderr, "end write error: %s\n", soundio_strerror(err));
                return;
            }

            frames_left -= frame_count;
        }
    }
}

AudioDevice::AudioDevice(SoundIo* soundio, int output_device) {
    // if output_device == -1, use default output device
    if (output_device < 0) {
        output_device = soundio_default_output_device_index(soundio);

        if (output_device < 0) {
            fprintf(stderr, "no output device found");
            exit(1);
        }
    }

    // create output device
    device = soundio_get_output_device(soundio, output_device);
    if (!device) {
        fprintf(stderr, "out of memory!");
        exit(1);
    }

    // create output stream
    outstream = soundio_outstream_create(device);
    outstream->software_latency = 0.05;
    outstream->sample_rate = 48000;
    outstream->format = SoundIoFormatFloat32NE;
    outstream->userdata = this;
    outstream->write_callback = soundio_write_callback;

    // initialize output stream
    int err;

    if ((err = soundio_outstream_open(outstream))) {
        fprintf(stderr, "unable to open device: %s\n", soundio_strerror(err));
        exit(1);
    }

    if (outstream->layout_error)
        fprintf(stderr, "unable to set channel layout: %s\n", soundio_strerror(outstream->layout_error));

    if ((err = soundio_outstream_start(outstream))) {
        fprintf(stderr, "unable to start device: %s\n", soundio_strerror(err));
        exit(1);
    }
}

AudioDevice::~AudioDevice()
{
    stop();
}

void AudioDevice::stop() {
    if (outstream) soundio_outstream_destroy(outstream);
    if (device) soundio_device_unref(device);
    outstream = nullptr;
    device = nullptr;

    if (audio_buffer != nullptr) {
        delete[] audio_buffer;
        audio_buffer = nullptr;
    }
}

int AudioDevice::sample_rate() const {
    return outstream->sample_rate;
}

int AudioDevice::num_channels() const {
    return outstream->layout.channel_count;
}

void AudioDevice::set_buffer_size(size_t new_size)
{
    buffer_size_change.new_size = new_size;
    buffer_size_change.change = true;
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
}

void ModuleBase::disconnect() {
    if (_output != nullptr) _output->remove_input(this);
    _output = nullptr;
}

void ModuleBase::remove_all_connections() {
    disconnect();

    for (ModuleBase* input : _inputs) {
        input->disconnect();
    }
}

float* ModuleBase::get_audio(size_t buffer_size, int sample_rate, int channel_count) {
    // audio buffer array to correct size
    if (_audio_buffer == nullptr || _audio_buffer_size != buffer_size * channel_count) {
        if (_audio_buffer != nullptr) delete[] _audio_buffer;
        _audio_buffer_size = buffer_size * channel_count;
        _audio_buffer = new float[_audio_buffer_size];
    }
    
    // accumulate inputs
    for (size_t i = 0; i < _inputs.size(); i++) {
        _input_arrays[i] = _inputs[i]->get_audio(buffer_size, sample_rate, channel_count);
    }

    // processing
    size_t num_inputs = _inputs.size();
    process(num_inputs > 0 ? &_input_arrays.front() : nullptr, _audio_buffer, num_inputs, buffer_size * channel_count, sample_rate, channel_count);

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

size_t DestinationModule::process(float** output) {
    if (_audio_buffer == nullptr || _prev_buffer_size != buffer_size * channel_count) {
        if (_audio_buffer != nullptr) delete[] _audio_buffer;
        _prev_buffer_size = buffer_size * channel_count;
        _audio_buffer = new float[_prev_buffer_size];
    }

    // accumulate inputs
    size_t num_inputs = _inputs.size();
    float** input_arrays = new float*[num_inputs];

    for (size_t i = 0; i < num_inputs; i++) {
        input_arrays[i] = _inputs[i]->get_audio(buffer_size, sample_rate, channel_count);
    }

    // combine all inputs into one buffer
    for (size_t i = 0; i < buffer_size * channel_count; i++) {
        _audio_buffer[i] = 0.0f;
        
        for (size_t j = 0; j < num_inputs; j++) {
            _audio_buffer[i] += input_arrays[j][i];
        }
    }

    delete[] input_arrays;

    *output = _audio_buffer;
    return buffer_size * channel_count;
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