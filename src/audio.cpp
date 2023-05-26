#include <exception>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <iostream>
#include "audio.h"
#include "sys.h"
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
    outstream->software_latency = 0.05;
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
    _has_interface(has_interface),
    id("")
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

bool ModuleBase::has_interface() const {
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
static double _mod(double a, double b) {
    return fmod(fmod(a, b) + b, b);
}

#include <math.h>
static constexpr double PI = 3.14159265359;
static constexpr double PI2 = 2.0f * PI;

WaveformSynth::WaveformSynth() : ModuleBase(true) {
    id = "synth.waveform";

    waveform_types[0] = Triangle;
    volume[0] = 0.5f;
    coarse[0] = 0;
    fine[0] = 0.0f;
    panning[0] = 0.0f;

    waveform_types[1] = Sawtooth;
    volume[1] = 0.5f;
    coarse[1] = 0.0;
    fine[1] = 6.0f;
    panning[1] = 0.0f;

    waveform_types[2] = Sine;
    volume[2] = 0.0f;
    coarse[2] = 0;
    fine[2] = 0.0f;
    panning[2] = 0.0f;
}

void WaveformSynth::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) {
    if (release < 0.001f) release = 0.001f;
    float t, env;
    float samples[3][2];

    for (size_t i = 0; i < buffer_size; i += channel_count) {
        // set both channels to zero
        for (size_t ch = 0; ch < channel_count; ch++) output[i + ch] = 0.0f;

        // compute all voices
        for (size_t j = 0; j < voices.size();) {
            Voice& voice = voices[j];

            env = sustain; // envelope value

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

            for (size_t osc = 0; osc < 3; osc++) {
                float freq = voice.freq * powf(2.0f, ((float)coarse[osc] + fine[osc] / 100.0f) / 12.0f);
                double phase = voice.phase[osc];
                double period = 1.0 / freq;
                float sample;

                float r_mult = (panning[osc] + 1.0f) / 2.0f;
                float l_mult = 1.0f - r_mult;

                switch (waveform_types[osc]) {
                    case Sine:
                        sample = sin(phase);
                        break;

                    case Triangle: {
                        double moda = phase / (PI2 * freq) - period / 4.0;
                        sample = (4.0 / period) * abs(_mod(moda, period) - period / 2.0) - 1.0;
                        break;
                    }

                    case Square:
                        sample = sin(phase) >= 0.0 ? 1.0 : -1.0;
                        break;

                    case Sawtooth:
                        sample = 2.0 * _mod(phase / PI2 + 0.5, 1.0) - 1.0;
                        break;

                    // a pulse wave: value[w] = (2.0f * _modf(phase / M_2PI + 0.5f, 1.3f) - 1.0f) > 0.0f ? 1.0f : -1.0f;
                }

                sample *= env * voice.volume * volume[osc];
                samples[osc][0] = sample * l_mult;
                samples[osc][1] = sample * r_mult;

                voice.phase[osc] += (PI2 * freq) / sample_rate;
            }

            output[i] += samples[0][0] + samples[1][0] + samples[2][0];
            output[i + 1] += samples[0][1] + samples[1][1] + samples[2][1];

            voice.time += 1.0 / sample_rate;
            j++;
        }
    }
}

void WaveformSynth::event(const NoteEvent& event) {
    if (event.kind == NoteEventKind::NoteOn) {
        NoteOnEvent event_data = event.note_on;
        //std::cout << "note on " << event_data.key << "\n";
        voices.push_back({
            event_data.key,
            event_data.freq,
            event_data.volume,
            { 0.0, 0.0, 0.0 },
            0.0,
            -1.0f,
            // undefined: release_env
        });
    
    } else if (event.kind == NoteEventKind::NoteOff) {
        NoteOffEvent event_data = event.note_off;
        //std::cout << "note off " << event_data.key << "\n";
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

static void render_slider(const char* id, const char* label, float* var, float max, float ramp, const char* fmt, float start = 0.0f) {
    static char display_buf[8];
    snprintf(display_buf, 8, fmt, *var);

    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s", label);
    ImGui::SameLine();
    
    float val = powf((*var - start) / max, 1.0f / ramp);
    ImGui::SliderFloat(id, &val, 0.0f, 1.0f, display_buf);
    *var = powf(val, ramp) * max + start;
}

bool WaveformSynth::render_interface(const char* channel_name) {
    if (!show_interface) return false;

    static const char* WAVEFORM_NAMES[] = {
        "Sine",
        "Square",
        "Sawtooth",
        "Triangle"
    };

    char window_name[128];
    snprintf(window_name, 128, "%s - Waveform Synth###%p", channel_name == nullptr ? "" : channel_name, this);

    if (ImGui::Begin(window_name, &show_interface, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize)) {
        char sep_text[] = "Oscillator 1";

        for (int osc = 0; osc < 3; osc++) {
            ImGui::PushID(osc);

            snprintf(sep_text, 13, "Oscillator %i", osc);
            ImGui::SeparatorText(sep_text);

            // waveform dropdown
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Waveform");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##channel_bus", WAVEFORM_NAMES[waveform_types[osc]]))
            {
                for (int i = 0; i < 4; i++) {
                    if (ImGui::Selectable(WAVEFORM_NAMES[i], i == waveform_types[osc])) waveform_types[osc] = static_cast<WaveformType>(i);

                    if (i == waveform_types[osc]) {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }

            ImGui::PushItemWidth(80.0f);

            // volume slider
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Vol");
            ImGui::SameLine();
            ImGui::SliderFloat("##volume", volume + osc, 0.0f, 1.0f);
            if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) volume[osc] = 1.0f;

            // panning slider
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Pan");
            ImGui::SameLine();
            ImGui::SliderFloat("##panning", panning + osc, -1.0f, 1.0f);
            if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) panning[osc] = 0.0f;

            // coarse slider
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Crs");
            ImGui::SameLine();
            ImGui::SliderInt("##coarse", coarse + osc, -24, 24);
            if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) coarse[osc] = 0;

            // fine slider
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Fne");
            ImGui::SameLine();
            ImGui::SliderFloat("##fine", fine + osc, -100.0f, 100.0f);
            if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) fine[osc] = 0.0f;

            ImGui::PopItemWidth();
            ImGui::PopID();
        }

        ImGui::SeparatorText("Envelope");
        ImGui::PushItemWidth(80.0f);

        // Attack slider
        render_slider("##attack", "Atk", &this->attack, 5.0f, 4.0f, "%.3f secs");
        if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) this->attack = 0.0f;

        // Decay slider
        ImGui::SameLine();
        render_slider("##decay", "Dky", &this->decay, 10.0f, 4.0f, "%.3f secs");
        if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) this->attack = 0.0f;

        // Sustain slider
        render_slider("##sustain", "Sus", &this->sustain, 1.0f, 1.0f, "%.3f");
        if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) this->attack = 1.0f;

        // Release slider
        ImGui::SameLine();
        render_slider("##release", "Rls", &this->release, 10.0f, 4.0f, "%.3f secs", 0.001f);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) this->attack = 0.001f;

        ImGui::PopItemWidth();
    } ImGui::End();

    return true;
}

struct WaveformSynthState {
    uint8_t waveform_types[3];
    float volume[3];
    float panning[3];
    int coarse[3];
    float fine[3];
};

size_t WaveformSynth::save_state(void** output) const {
    WaveformSynthState* state = new WaveformSynthState;

    for (size_t osc = 0; osc < 3; osc++) {
        state->waveform_types[osc] = static_cast<uint8_t>(waveform_types[osc]);
        state->volume[osc] = swap_little_endian(volume[osc]);
        state->panning[osc] = swap_little_endian(panning[osc]);
        state->coarse[osc] = swap_little_endian(coarse[osc]);
        state->fine[osc] = swap_little_endian(fine[osc]);
    }

    *output = state;
    return sizeof(WaveformSynthState);
}

bool WaveformSynth::load_state(void* state_ptr, size_t size) {
    if (size != sizeof(WaveformSynthState)) return false;
    WaveformSynthState* state = (WaveformSynthState*)state_ptr;

    for (size_t osc = 0; osc < 3; osc++) {
        waveform_types[osc] = static_cast<WaveformType>(state->waveform_types[osc]);
        volume[osc] = swap_little_endian(state->volume[osc]);
        panning[osc] = swap_little_endian(state->panning[osc]);
        coarse[osc] = swap_little_endian(state->coarse[osc]);
        fine[osc] = swap_little_endian(state->fine[osc]);
    }

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
    id = "effect.volume";
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

struct VolumeModuleState {
    float volume, panning;
};

size_t VolumeModule::save_state(void** output) const {
    VolumeModuleState* state = new VolumeModuleState;

    state->volume = swap_little_endian(volume);
    state->panning = swap_little_endian(panning);

    *output = state;
    return sizeof(VolumeModuleState);
}

bool VolumeModule::load_state(void* state_ptr, size_t size) {
    if (size != sizeof(VolumeModuleState)) return false;
    VolumeModuleState* state = (VolumeModuleState*)state_ptr;
    
    volume = swap_little_endian(state->volume);
    panning = swap_little_endian(state->panning);

    return true;
}