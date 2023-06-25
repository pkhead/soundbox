#include "waveform.h"
#include <cstdint>
#include <cstdlib>
#include <imgui.h>
#include "../sys.h"
#include <math.h>
#include "../song.h"

using namespace audiomod;

static double _mod(double a, double b) {
    return fmod(fmod(a, b) + b, b);
}

static constexpr double PI = 3.14159265359;
static constexpr double PI2 = 2.0f * PI;

static const size_t NOISE_DATA_SIZE = 1 << 16;
static float NOISE_DATA[1 << 16];
static bool PREPROCESSED_DATA_READY = false;

WaveformSynth::WaveformSynth() : ModuleBase(true) {
    id = "synth.waveform";
    name = "Waveform Synth";

    // generate static data
    if (!PREPROCESSED_DATA_READY)
    {
        PREPROCESSED_DATA_READY = true;

        for (size_t i = 0; i < NOISE_DATA_SIZE; i++)
        {
            NOISE_DATA[i] = (double)rand() / RAND_MAX * 2.0 - 1.0;
        }
    }

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

    for (size_t i = 0; i < MAX_VOICES; i++)
        voices[i].active = false;
}

void WaveformSynth::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) {
    if (release < 0.001f) release = 0.001f;
    float t, env;
    float samples[3][2];

    for (size_t i = 0; i < buffer_size; i += channel_count) {
        // set both channels to zero
        for (size_t ch = 0; ch < channel_count; ch++) output[i + ch] = 0.0f;

        // compute all voices
        for (size_t j = 0; j < MAX_VOICES; j++) {
            Voice& voice = voices[j];
            if (!voice.active) continue;

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
                    voice.active = false;
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

                    case Noise:
                        sample = NOISE_DATA[(int)(phase * 4.0f) % NOISE_DATA_SIZE];
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
        }
    }
}

void WaveformSynth::event(const NoteEvent& event) {
    if (event.kind == NoteEventKind::NoteOn) {
        NoteOnEvent event_data = event.note_on;
        
        // create new voice in first found empty slot
        // if there are no empty slots, replace the first voice in memory
        Voice* voice = voices+0;

        for (size_t i = 0; i < MAX_VOICES; i++)
        {
            if (!voices[i].active)
            {
                voice = voices+i;
                break;
            }
        }

        float key_freq;
        if (song->get_key_frequency(event_data.key, &key_freq))
        {
            *voice = {
                true,
                event_data.key,
                key_freq,
                event_data.volume,
                { 0.0, 0.0, 0.0 },
                0.0,
                -1.0f,
                // undefined: release_env
            };
        }
    
    } else if (event.kind == NoteEventKind::NoteOff) {
        NoteOffEvent event_data = event.note_off;
        
        for (size_t i = 0; i < MAX_VOICES; i++) {
            Voice& voice = voices[i];

            if (voice.active && voice.key == event_data.key && voice.release_time < 0.0f) {
                voice.release_time = voice.time;

                // calculate envelope at release time
                voice.release_env = sustain;
                float t;

                if (voice.time < attack) {
                    voice.release_env = voice.time / attack;
                } else if (voice.time < decay) {
                    t = (voice.time - attack) / decay;
                    if (t > 1.0f) t = 1.0f;
                    voice.release_env = (sustain - 1.0f) * t + 1.0f;
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

void WaveformSynth::_interface_proc() {
    static const char* WAVEFORM_NAMES[] = {
        "Sine",
        "Square",
        "Sawtooth",
        "Triangle",
        "Noise",
    };
    
    const float slider_width = ImGui::GetTextLineHeight() * 6.0f;
    char sep_text[] = "Oscillator 1";

    for (int osc = 0; osc < 3; osc++) {
        ImGui::PushID(osc);

        snprintf(sep_text, 13, "Oscillator %i", osc + 1);
        ImGui::SeparatorText(sep_text);

        // waveform dropdown
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Waveform");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("##channel_bus", WAVEFORM_NAMES[waveform_types[osc]]))
        {
            for (int i = 0; i < 5; i++) {
                if (ImGui::Selectable(WAVEFORM_NAMES[i], i == waveform_types[osc])) waveform_types[osc] = static_cast<WaveformType>(i);

                if (i == waveform_types[osc]) {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }

        ImGui::PushItemWidth(slider_width);

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
    ImGui::PushItemWidth(slider_width);

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
}

// TODO: in a later version, remove the alpha v1 version
struct WaveformSynthState_v1 {
    uint8_t waveform_types[3];
    float volume[3];
    float panning[3];
    int coarse[3];
    float fine[3];
};

// TODO: remove this too
struct WaveformSynthState_v2 {
    uint8_t waveform_types[3];
    float volume[3];
    float panning[3];
    int coarse[3];
    float fine[3];

    float atk, dky, sus, rls;
};

struct WaveformSynthState_v3
{
    uint8_t version;
    uint8_t waveform_types[3];
    float volume[3];
    float panning[3];
    int32_t coarse[3];
    float fine[3];

    float atk, dky, sus, rls;
};

void WaveformSynth::save_state(std::ostream& ostream) const {
    // write version
    push_bytes<uint8_t>(ostream, 0);

    // write oscillator config
    for (size_t osc = 0; osc < 3; osc++)
    {
        push_bytes<uint8_t>(ostream, waveform_types[osc]);
        push_bytes<float>(ostream, volume[osc]);
        push_bytes<float>(ostream, panning[osc]);
        push_bytes<int32_t>(ostream, coarse[osc]);
        push_bytes<float>(ostream, fine[osc]);
    }

    // write envelope config
    push_bytes<float>(ostream, attack);
    push_bytes<float>(ostream, decay);
    push_bytes<float>(ostream, sustain);
    push_bytes<float>(ostream, release);
}

bool WaveformSynth::load_state(std::istream& istream, size_t size)
{
    // get version
    uint8_t version = pull_bytesr<uint8_t>(istream);
    if (version != 0) return false; // invalid version

    // read oscillator config
    for (size_t osc = 0; osc < 3; osc++)
    {
        waveform_types[osc] = static_cast<WaveformType>(pull_bytesr<uint8_t>(istream));
        volume[osc] = pull_bytesr<float>(istream);
        panning[osc] = pull_bytesr<float>(istream);
        coarse[osc] = pull_bytesr<int32_t>(istream);
        fine[osc] = pull_bytesr<float>(istream);
    }

    // read envelope config
    attack = pull_bytesr<float>(istream);
    decay = pull_bytesr<float>(istream);
    sustain = pull_bytesr<float>(istream);
    release = pull_bytesr<float>(istream);

    return true;
}