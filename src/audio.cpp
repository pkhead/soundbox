#include <exception>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <iostream>
#include "audio.h"
#include "../imgui/imgui.h"

static void write_callback(SoundIoOutStream* outstream, int frame_count_min, int frame_count_max) {
    AudioDevice* handle = (AudioDevice*)outstream->userdata;
    handle->write(outstream, frame_count_min, frame_count_max);
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
    outstream->software_latency = 0.02;
    outstream->sample_rate = 48000;
    outstream->format = SoundIoFormatFloat32NE;
    outstream->userdata = this;
    outstream->write_callback = write_callback;

    // initialize output stream
    int err;

    if ((err = soundio_outstream_open(outstream))) {
        fprintf(stderr, "unable to open device: %s\n", soundio_strerror(err));
        exit(1);
    }

    if (outstream->layout_error)
        fprintf(stderr, "unable to set channel layout: %s\n", soundio_strerror(outstream->layout_error));

    // create ring buffer
    buf_capacity = (size_t)(outstream->software_latency * outstream->sample_rate) * 4;
    size_t capacity = buf_capacity * outstream->bytes_per_frame;
    ring_buffer = soundio_ring_buffer_create(soundio, capacity);
    buf_capacity = soundio_ring_buffer_capacity(ring_buffer);
    printf("buf_capacity: %li\n", buf_capacity);

    if ((err = soundio_outstream_start(outstream))) {
        fprintf(stderr, "unable to start device: %s\n", soundio_strerror(err));
        exit(1);
    }
}

AudioDevice::~AudioDevice() {
    soundio_outstream_destroy(outstream);
    soundio_device_unref(device);
}

bool AudioDevice::queue(const float* buffer, const size_t buffer_size) {
    if (buffer_size > (size_t)soundio_ring_buffer_free_count(ring_buffer)) return true;

    char* write_ptr = soundio_ring_buffer_write_ptr(ring_buffer);
    memcpy(write_ptr, buffer, buffer_size);
    soundio_ring_buffer_advance_write_ptr(ring_buffer, buffer_size);

    return false;
}

int AudioDevice::sample_rate() const {
    return outstream->sample_rate;
}

int AudioDevice::num_channels() const {
    return outstream->layout.channel_count;
}

int AudioDevice::num_queued_frames() const {
    size_t bytes_queued = buf_capacity - soundio_ring_buffer_free_count(ring_buffer);
    return bytes_queued / outstream->bytes_per_frame;
}

void AudioDevice::write(SoundIoOutStream* stream, int frame_count_min, int frame_count_max) {
    SoundIoChannelArea* areas;
    int frames_left, frame_count, err;

    char* read_ptr = soundio_ring_buffer_read_ptr(ring_buffer);
    int fill_bytes = soundio_ring_buffer_fill_count(ring_buffer);
    int fill_count = fill_bytes / stream->bytes_per_frame;

    if (fill_count < frame_count_min) {
        // ring buffer doesn't have enough data, fill with zeroes
        while (true) {
            frames_left = frame_count_min;

            if (frame_count_min <= 0) return;
            if ((err = soundio_outstream_begin_write(stream, &areas, &frame_count))) {
                fprintf(stderr, "begin write error: %s\n", soundio_strerror(err));
                return;
            }
            if (frame_count <= 0) return;

            for (int frame = 0; frame < frame_count; frame++) {
                for (int ch = 0; ch < stream->layout.channel_count; ch++) {
                    memset(areas[ch].ptr, 0, stream->bytes_per_sample);
                    areas[ch].ptr += areas[ch].step;
                }
            }

            if ((err = soundio_outstream_end_write(stream))) {
                fprintf(stderr, "end write error: %s\n", soundio_strerror(err));
                return;
            }
        }
    }

    int read_count = frame_count_max < fill_count ? frame_count_max : fill_count;
    frames_left = read_count;

    while (frames_left > 0) {
        frame_count = frames_left;
        if ((err = soundio_outstream_begin_write(stream, &areas, &frame_count))) {
            fprintf(stderr, "begin write error: %s\n", soundio_strerror(err));
            return;
        }
        if (frame_count <= 0) break;

        for (int frame = 0; frame < frame_count; frame++) {
            for (int ch = 0; ch < stream->layout.channel_count; ch++) {
                memcpy(areas[ch].ptr, read_ptr, stream->bytes_per_sample);
                areas[ch].ptr += areas[ch].step;
                read_ptr += stream->bytes_per_sample;
            }
        }

        if ((err = soundio_outstream_end_write(stream))) {
            fprintf(stderr, "end write error: %s\n", soundio_strerror(err));
            return;
        }

        frames_left -= frame_count;
    }

    soundio_ring_buffer_advance_read_ptr(ring_buffer, read_count * stream->bytes_per_frame);
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
}

bool ModuleOutputTarget::remove_input(ModuleBase* module) {
    for (auto it = _inputs.begin(); it != _inputs.end(); it++) {
        if (*it == module) {
            _inputs.erase(it);
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
    _has_interface(has_interface)
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
    if (this->_output != nullptr) this->_output->remove_input(this);
    this->_output = nullptr;
}

void ModuleBase::remove_all_connections() {
    disconnect();

    for (ModuleBase* input : _inputs) {
        remove_input(input);
    }

    _inputs.clear();
}

float* ModuleBase::get_audio(size_t buffer_size, int sample_rate, int channel_count) {
    if (_audio_buffer == nullptr || _audio_buffer_size != buffer_size * channel_count) {
        if (_audio_buffer != nullptr) delete[] _audio_buffer;
        _audio_buffer_size = buffer_size * channel_count;
        _audio_buffer = new float[_audio_buffer_size];
    }

    // accumulate inputs
    float** input_arrays = new float*[_inputs.size()];

    for (size_t i = 0; i < _inputs.size(); i++) {
        input_arrays[i] = _inputs[i]->get_audio(buffer_size, sample_rate, channel_count);
    }

    // processing
    process(input_arrays, _audio_buffer, _inputs.size(), buffer_size * channel_count, sample_rate, channel_count);

    delete[] input_arrays;
    return _audio_buffer;
}

inline bool ModuleBase::has_interface() const {
    return _has_interface;
}










//////////////////////////
//  DESTINATION MODULE  //
//////////////////////////

DestinationModule::DestinationModule(AudioDevice& device, size_t buffer_size) : time(0.0), device(device), buffer_size(buffer_size) {
    _audio_buffer = nullptr;
    _prev_buffer_size = 0;
}

DestinationModule::~DestinationModule() {
    if (_audio_buffer != nullptr) delete[] _audio_buffer;
}

size_t DestinationModule::process(float** output) {
    int sample_rate = device.sample_rate();
    int channel_count = device.num_channels();

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











//////////////////////////
//    WAVEFORM SYNTH    //
//////////////////////////

#include <math.h>
static constexpr double PI = 3.14159265359;
static constexpr double PI2 = 2.0f * PI;

WaveformSynth::WaveformSynth() : ModuleBase(true) {
    time = 0.0;
    waveform_type = Sine;
}

void WaveformSynth::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) {
    if (release < 0.001f) release = 0.001f;
    float t;

    for (size_t i = 0; i < buffer_size; i += channel_count) {
        // set both channels to zero
        for (size_t ch = 0; ch < channel_count; ch++) output[i + ch] = 0.0f;

        // compute all voices
        for (size_t j = 0; j < voices.size();) {
            Voice& voice = voices[j];

            float env = sustain; // envelope value

            if (voice.time < attack) {
                env = voice.time / attack;
            } else if (voice.time < decay) {
                t = (voice.time - attack) / decay;
                if (t > 1.0f) t = 1.0f;
                env = (sustain - 1.0f) * t + 1.0f;
            }

            // if note is in the release state
            if (voice.release_time >= 0.0f) {
                t = (voice.time - voice.release_time) / release;
                if (t > 1.0f) {
                    // time has gone past the release envelope, officially end the note
                    voices.erase(voices.begin() + j);
                    continue;
                }

                env = (1.0f - t) * voice.release_env;
            }

            float sample = sin((PI2 * voice.freq) * voice.time) * (env * voice.volume);

            for (size_t ch = 0; ch < channel_count; ch++) {
                output[i + ch] += sample;
            }

            voice.time += 1.0 / sample_rate;
            j++;
        }
    }
}

void WaveformSynth::event(const NoteEvent& event) {
    if (event.kind == NoteEventKind::NoteOn) {
        NoteOnEvent event_data = event.note_on;
        std::cout << "note on " << event_data.key << "\n";
        voices.push_back({
            event_data.key,
            event_data.freq,
            event_data.volume,
            0.0,
            -1.0f,
        });
    
    } else if (event.kind == NoteEventKind::NoteOff) {
        NoteOffEvent event_data = event.note_off;
        std::cout << "note off " << event_data.key << "\n";
        for (auto it = voices.begin(); it != voices.end(); it++) {
            if (it->key == event_data.key && it->release_time < 0.0f) {
                it->release_time = it->time;

                // calculate envelope at release time
                it->release_env = sustain;
                float t;

                if (it->time < attack) {
                    it->release_env = it->time / attack;
                } else if (it->time < decay) {
                    t = (it->time - attack) / decay;
                    if (t > 1.0f) t = 1.0f;
                    it->release_env = (sustain - 1.0f) * t + 1.0f;
                }

                break;
            }
        }
    }
}

static void render_slider(const char* id, float* var, float max, float ramp, const char* fmt, float start = 0.0f) {
    float val = powf((*var - start) / max, 1.0f / ramp);
    ImGui::VSliderFloat(id, ImVec2(18, 80), &val, 0.0f, 1.0f, "");
    *var = powf(val, ramp) * max + start;

    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
        ImGui::SetTooltip(fmt, *var);
    }
}

bool WaveformSynth::render_interface() {
    if (!show_interface) return false;

    char buf[128];
    snprintf(buf, 128, "Waveform Synth##%x", this);

    if (ImGui::Begin(buf, &show_interface, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking)) {
        // Attack slider
        render_slider("A", &this->attack, 5.0f, 4.0f, "%.3f secs");

        // Decay slider
        ImGui::SameLine();
        render_slider("D", &this->decay, 10.0f, 4.0f, "%.3f secs");

        // Sustain slider
        ImGui::SameLine();
        render_slider("S", &this->sustain, 1.0f, 1.0f, "%.3f");

        // Release slider
        ImGui::SameLine();
        render_slider("R", &this->release, 10.0f, 4.0f, "%.3f secs", 0.001f);
    }
        ImGui::End();

    return true;
}
















//////////////////////////
//    VOLUME MODULE     //
//////////////////////////
inline int sign(float v) {
    return v >= 0.0f ? 1 : -1; 
}

inline bool is_zero_crossing(float prev, float next) {
    return (prev == 0.0f && next == 0.0f) || (sign(prev) != sign(next));
}

VolumeModule::VolumeModule() : ModuleBase(false) {
    cur_volume[0] = volume;
    cur_volume[1] = volume;
    last_sample[0] = 0.0f;
    last_sample[1] = 0.0f;
}

void VolumeModule::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) {
    float r_mult = (panning + 1.0f) / 2.0f;
    float l_mult = 1.0f - r_mult;

    for (size_t i = 0; i < buffer_size; i += channel_count) {
        // set both channels to zero
        output[i] = 0.0f;
        output[i + 1] = 0.0f;

        for (size_t j = 0; j < num_inputs; j++) {
            if (is_zero_crossing(last_sample[0], inputs[j][i])) cur_volume[0] = volume * l_mult;
            if (is_zero_crossing(last_sample[1], inputs[j][i + 1])) cur_volume[1] = volume * r_mult;

            output[i] += inputs[j][i] * cur_volume[0];
            output[i + 1] += inputs[j][i + 1] * cur_volume[1];


            last_sample[0] = output[i];
            last_sample[1] = output[i + 1];
        }
    }
}