#include "waveform.h"
#include <cstdint>
#include <cstdlib>
#include <imgui.h>
#include "../sys.h"
#include <math.h>
#include "../song.h"
#include "../util.h"

using namespace audiomod;

static double _mod(double a, double b) {
    return fmod(fmod(a, b) + b, b);
}

static constexpr double PI = 3.14159265359;
static constexpr double PI2 = 2.0f * PI;

static const size_t NOISE_DATA_SIZE = 1 << 16;
static float NOISE_DATA[1 << 16];
static bool PREPROCESSED_DATA_READY = false;

WaveformSynth::WaveformSynth(ModuleContext& modctx)
:   ModuleBase(true), modctx(modctx),
    event_queue(sizeof(NoteEvent), MAX_VOICES*2),
    state_queue(sizeof(module_state_t), 2)
{
    id = "synth.waveform";
    name = "Waveform Synth";

    // generate static noise data
    if (!PREPROCESSED_DATA_READY)
    {
        PREPROCESSED_DATA_READY = true;

        for (size_t i = 0; i < NOISE_DATA_SIZE; i++)
        {
            NOISE_DATA[i] = (double)rand() / RAND_MAX * 2.0 - 1.0;
        }
    }

    ui_state.waveform_types[0] = Triangle;
    ui_state.volume[0] = 0.5f;
    ui_state.coarse[0] = 0;
    ui_state.fine[0] = 0.0f;
    ui_state.panning[0] = 0.0f;

    ui_state.waveform_types[1] = Sawtooth;
    ui_state.volume[1] = 0.5f;
    ui_state.coarse[1] = 0.0;
    ui_state.fine[1] = 6.0f;
    ui_state.panning[1] = 0.0f;

    ui_state.waveform_types[2] = Sine;
    ui_state.volume[2] = 0.0f;
    ui_state.coarse[2] = 0;
    ui_state.fine[2] = 0.0f;
    ui_state.panning[2] = 0.0f;

    process_state = ui_state;

    for (size_t i = 0; i < MAX_VOICES; i++)
        voices[i].active = false;
}

static double poly_blep(double t, double inc)
{
    double dt = inc / PI2;
    // 0 <= t < 1
    if (t < dt) {
        t /= dt;
        return t+t - t*t - 1.0;
    }
    // -1 < t < 0
    else if (t > 1.0 - dt) {
        t = (t - 1.0) / dt;
        return t*t + t+t + 1.0;
    }
    // 0 otherwise
    else return 0.0;
}

void WaveformSynth::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) {
    // obtain state from ui thread
    while (true)
    {
        auto handle = state_queue.read();
        if (!handle) break;
        
        assert(handle.size() == sizeof(module_state_t));
        handle.read(&process_state, sizeof(module_state_t));
    }
    
    float release = process_state.release;
    float sustain = process_state.sustain;
    float attack = process_state.attack;
    float decay = process_state.decay;
    
    if (release < 0.001f) release = 0.001f;
    float t, env;
    float samples[3][2];

    for (size_t i = 0; i < buffer_size; i += modctx.num_channels) {
        // set both channels to zero
        for (size_t ch = 0; ch < modctx.num_channels; ch++) output[i + ch] = 0.0f;

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
                float freq = voice.freq * powf(2.0f, ((float)process_state.coarse[osc] + process_state.fine[osc] / 100.0f) / 12.0f);
                double phase = voice.phase[osc];
                double period = 1.0 / freq;
                float sample;

                float r_mult = (process_state.panning[osc] + 1.0f) / 2.0f;
                float l_mult = 1.0f - r_mult;

                double increment = (PI2 * freq) / modctx.sample_rate;

                switch (process_state.waveform_types[osc]) {
                    case Sine:
                        sample = sin(phase);
                        break;

                    case Square:
                    case Triangle: // triangle is an integrated square wave
                        sample = phase < PI ? 1.0 : -1.0;
                        sample += poly_blep(phase / PI2, increment);
                        sample -= poly_blep(fmod(phase / PI2 + 0.5,1.0), increment);

                        if (process_state.waveform_types[osc] == Triangle)
                        {
                            // this doesn't quite make an accurate triangle wave,
                            // though it sounds enough like one. just in case, i'm
                            // keeping the code to make an actual triangle wave, just
                            // commented out
                            sample = increment * sample + (1 - increment) * voice.last_sample[osc];
                            voice.last_sample[osc] = sample;
                        }

                        break;
                    
                    /*case Triangle: {
                        sample = -1.0 + (2.0 * phase / PI2);
                        sample = 2.0 * (fabs(sample) - 0.5);
                        break;
                    }*/

                    case Sawtooth:
                        sample = (2.0 * phase / PI2) - 1.0;
                        sample -= poly_blep(phase / PI2, increment);
                        break;

                    // 25% pulse wave
                    case Pulse: {
                        // phase-shift
                        double mphase = _mod((phase + PI/2.0), PI2);

                        double a = phase / PI - 1.0;
                        a -= poly_blep(phase / PI2, increment);
                        double b = mphase / PI - 1.0;
                        b -= poly_blep(mphase / PI2, increment);
                        sample = a - b - 0.5;
                        break;
                    }

                    case Noise:
                        sample = NOISE_DATA[(int)(voice.time * freq * 4.0f) % NOISE_DATA_SIZE];
                        break;

                    // a pulse wave: value[w] = (2.0f * _modf(phase / M_2PI + 0.5f, 1.3f) - 1.0f) > 0.0f ? 1.0f : -1.0f;
                }

                sample *= env * voice.volume * process_state.volume[osc];
                samples[osc][0] = sample * l_mult;
                samples[osc][1] = sample * r_mult;

                voice.phase[osc] += increment;
                if (voice.phase[osc] > PI2)
                    voice.phase[osc] -= PI2;
            }

            output[i] += samples[0][0] + samples[1][0] + samples[2][0];
            output[i + 1] += samples[0][1] + samples[1][1] + samples[2][1];

            voice.time += 1.0 / modctx.sample_rate;
        }
    }
}

void WaveformSynth::event(const NoteEvent& event) {
    if (event.kind == NoteEventKind::NoteOn) {
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
        if (song->get_key_frequency(event.key, &key_freq))
        {
            *voice = {
                true,
                event.key,
                key_freq,
                event.volume,
                { 0.0, 0.0, 0.0 },
                0.0,
                -1.0f,
                0.0f,
                { 0.0, 0.0, 0.0 }
            };
        }
    
    } else if (event.kind == NoteEventKind::NoteOff) {
        for (size_t i = 0; i < MAX_VOICES; i++) {
            Voice& voice = voices[i];

            if (voice.active && voice.key == event.key && voice.release_time < 0.0f) {
                voice.release_time = voice.time;

                // calculate envelope at release time
                voice.release_env = process_state.sustain;
                float t;

                if (voice.time < process_state.attack) {
                    voice.release_env = voice.time / process_state.attack;
                } else if (voice.time < process_state.decay) {
                    t = (voice.time - process_state.attack) / process_state.decay;
                    if (t > 1.0f) t = 1.0f;
                    voice.release_env = (process_state.sustain - 1.0f) * t + 1.0f;
                }

                break;
            }
        }
    }
}

void WaveformSynth::queue_event(const NoteEvent& event)
{
    event_queue.post(&event, sizeof(event));
}

void WaveformSynth::flush_events()
{
    while (true) {
        auto handle = event_queue.read();
        if (!handle) break;

        // ERROR
        assert(handle.size() == sizeof(NoteEvent));
        NoteEvent ev;
        handle.read(&ev, sizeof(NoteEvent));
        event(ev);
    }
}

static void render_slider(const char* id, const char* label, float* var, float max, bool log, const char* fmt, float start = 0.0f) {
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s", label);
    ImGui::SameLine();
    ImGui::SliderFloat(
        id, var, start, max, fmt,
        (log ? ImGuiSliderFlags_Logarithmic : 0) | ImGuiSliderFlags_NoRoundToFormat
    );
}

void WaveformSynth::_interface_proc() {
    static const char* WAVEFORM_NAMES[] = {
        "Sine",
        "Square",
        "Sawtooth",
        "Triangle",
        "Pulse",
        "Noise",
    };

    ImGuiStyle& style = ImGui::GetStyle();
    
    const float slider_width = ImGui::GetTextLineHeight() * 6.0f;
    char sep_text[] = "Oscillator 1";

    ImGui::AlignTextToFramePadding();
    ImGui::BeginGroup();

    for (int osc = 0; osc < 3; osc++) {
        ImGui::PushID(osc);
        
        ImGui::AlignTextToFramePadding();
        snprintf(sep_text, 13, "Osc %i", osc + 1);
        ImGui::Text("%s", sep_text);

        // waveform dropdown
        ImGui::SameLine();
        ImGui::SetNextItemWidth(slider_width * 2 + style.ItemSpacing.x);
        if (ImGui::BeginCombo("##channel_bus", WAVEFORM_NAMES[ui_state.waveform_types[osc]]))
        {
            for (int i = 0; i < 6; i++) {
                if (ImGui::Selectable(WAVEFORM_NAMES[i], i == ui_state.waveform_types[osc])) ui_state.waveform_types[osc] = static_cast<WaveformType>(i);

                if (i == ui_state.waveform_types[osc]) {
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
        ImGui::SliderFloat("##volume", ui_state.volume + osc, 0.0f, 1.0f);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) ui_state.volume[osc] = 1.0f;

        // panning slider
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Pan");
        ImGui::SameLine();
        ImGui::SliderFloat("##panning", ui_state.panning + osc, -1.0f, 1.0f);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) ui_state.panning[osc] = 0.0f;

        // coarse slider
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Crs");
        ImGui::SameLine();
        ImGui::SliderInt("##coarse", ui_state.coarse + osc, -24, 24);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) ui_state.coarse[osc] = 0;

        // fine slider
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Fne");
        ImGui::SameLine();
        ImGui::SliderFloat("##fine", ui_state.fine + osc, -100.0f, 100.0f);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) ui_state.fine[osc] = 0.0f;

        ImGui::PopItemWidth();
        ImGui::PopID();
    }

    ImGui::EndGroup();
    ImGui::SameLine();
    ImGui::BeginGroup();

    ImGui::Text("Amplitude Envelope");
    ImGui::PushItemWidth(slider_width);

    // Attack slider
    render_slider("##attack", "Atk", &ui_state.attack, 5.0f, false, "%.3f s");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) ui_state.attack = 0.0f;

    // Decay slider
    ImGui::SameLine();
    render_slider("##decay", "Dky", &ui_state.decay, 5.0f, false, "%.3f s");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) ui_state.attack = 0.0f;

    // Sustain slider
    render_slider("##sustain", "Sus", &ui_state.sustain, 1.0f, false, "%.3f");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) ui_state.attack = 1.0f;

    // Release slider
    ImGui::SameLine();
    render_slider("##release", "Rls", &ui_state.release, 5.0f, false, "%.3f s", 0.001f);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) ui_state.attack = 0.001f;

    ImGui::EndGroup();

    ImGui::PopItemWidth();

    // send state to process thread
    state_queue.post(&ui_state, sizeof(ui_state));
}

void WaveformSynth::save_state(std::ostream& ostream) {
    // write version
    push_bytes<uint8_t>(ostream, 0);

    // write oscillator config
    for (size_t osc = 0; osc < 3; osc++)
    {
        push_bytes<uint8_t>(ostream, ui_state.waveform_types[osc]);
        push_bytes<float>(ostream, ui_state.volume[osc]);
        push_bytes<float>(ostream, ui_state.panning[osc]);
        push_bytes<int32_t>(ostream, ui_state.coarse[osc]);
        push_bytes<float>(ostream, ui_state.fine[osc]);
    }

    // write envelope config
    push_bytes<float>(ostream, ui_state.attack);
    push_bytes<float>(ostream, ui_state.decay);
    push_bytes<float>(ostream, ui_state.sustain);
    push_bytes<float>(ostream, ui_state.release);
}

bool WaveformSynth::load_state(std::istream& istream, size_t size)
{
    // get version
    uint8_t version = pull_bytesr<uint8_t>(istream);
    if (version != 0) return false; // invalid version

    // read oscillator config
    for (size_t osc = 0; osc < 3; osc++)
    {
        ui_state.waveform_types[osc] = static_cast<WaveformType>(pull_bytesr<uint8_t>(istream));
        ui_state.volume[osc] = pull_bytesr<float>(istream);
        ui_state.panning[osc] = pull_bytesr<float>(istream);
        ui_state.coarse[osc] = pull_bytesr<int32_t>(istream);
        ui_state.fine[osc] = pull_bytesr<float>(istream);
    }

    // read envelope config
    ui_state.attack = pull_bytesr<float>(istream);
    ui_state.decay = pull_bytesr<float>(istream);
    ui_state.sustain = pull_bytesr<float>(istream);
    ui_state.release = pull_bytesr<float>(istream);
    
    state_queue.post(&ui_state, sizeof(ui_state));

    return true;
}