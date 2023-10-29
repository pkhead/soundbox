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

WaveformSynth::Voice::Voice()
{
    phase[0] = phase[1] = phase[2] = 0.0f;
    last_sample[0] = last_sample[1] = last_sample[2] = 0.0f;
}

WaveformSynth::Voice::Voice(int key, float freq, float volume)
:   key(key), freq(freq), volume(volume), active(true)
{}

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

    ui_state.filter_type = LowPassFilter;
    ui_state.filt_freq = modctx.sample_rate / 2.5f;
    ui_state.filt_reso = 0.0f;

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

    ADSR amp_env_params = process_state.amp_env;
    ADSR filt_env_params = process_state.filt_env;

    if (amp_env_params.release < 0.001f) amp_env_params.release = 0.001f;
    if (filt_env_params.release < 0.001f) filt_env_params.release = 0.001f;

    float samples[3][2];

    // setup filter
    float reso_linear = db_to_mult(process_state.filt_reso);

    for (size_t i = 0; i < buffer_size; i += modctx.num_channels) {
        // set both channels to zero
        for (size_t ch = 0; ch < modctx.num_channels; ch++) output[i + ch] = 0.0f;

        // compute all voices
        for (size_t j = 0; j < MAX_VOICES; j++) {
            Voice& voice = voices[j];
            if (!voice.active) continue;

            float amp_env;
            if (voice.amp_env.compute(voice.time, amp_env, amp_env_params))
            {
                // note ended
                voice.active = false;
                break;
            }
            amp_env *= amp_env; // square envelope amount so it sounds smoother

            float filt_env;
            voice.filt_env.compute(voice.time, filt_env, filt_env_params);
            filt_env *= filt_env; // same for filter

            // setup filter
            float filt_freq = util::lerp(process_state.filt_freq, process_state.filt_freq * filt_env, process_state.filt_amount);
            if (filt_freq < 20.0f) filt_freq = 20.0f; // going too low on frequency will do... Something

            switch (process_state.filter_type)
            {
                case LowPassFilter:
                    voice.filter[0].low_pass(modctx.sample_rate, filt_freq, reso_linear);
                    voice.filter[1].low_pass(modctx.sample_rate, filt_freq, reso_linear);
                    break;

                case HighPassFilter:
                    voice.filter[0].high_pass(modctx.sample_rate, filt_freq, reso_linear);
                    voice.filter[1].high_pass(modctx.sample_rate, filt_freq, reso_linear);
                    break;

                case BandPassFilter:
                    // TODO: Band pass filter
                    break;
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

                sample *= amp_env * voice.volume * process_state.volume[osc];
                samples[osc][0] = sample * l_mult;
                samples[osc][1] = sample * r_mult;

                voice.phase[osc] += increment;
                if (voice.phase[osc] > PI2)
                    voice.phase[osc] -= PI2;
            }

            output[i] += samples[0][0] + samples[1][0] + samples[2][0];
            output[i + 1] += samples[0][1] + samples[1][1] + samples[2][1];

            voice.filter[0].process(&output[i]);
            voice.filter[1].process(&output[i+1]);

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
            *voice = Voice(event.key, key_freq, event.volume);
        }
    
    } else if (event.kind == NoteEventKind::NoteOff) {
        for (size_t i = 0; i < MAX_VOICES; i++) {
            Voice& voice = voices[i];

            if (voice.active && voice.key == event.key && !voice.amp_env.is_released()) {
                voice.amp_env.release(voice.time, process_state.amp_env);
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

    static const char* FILTER_NAMES[] = {
        "Low Pass",
        "High Pass",
        "Band Pass"
    };

    ImGuiStyle& style = ImGui::GetStyle();
    
    const float slider_width = ImGui::GetTextLineHeight() * 6.0f;
    const float combo_width = 2 * (ImGui::CalcTextSize("XXX").x + style.ItemSpacing.x + slider_width);

    char sep_text[] = "Osc X";

    ImGui::AlignTextToFramePadding();
    ImGui::BeginGroup();

    for (int osc = 0; osc < 3; osc++) {
        ImGui::PushID(osc);
        
        ImGui::AlignTextToFramePadding();
        snprintf(sep_text, sizeof(sep_text), "Osc %i", osc + 1);
        ImGui::Text("%s", sep_text);

        // waveform dropdown
        ImGui::SameLine();
        ImGui::SetNextItemWidth(combo_width - ImGui::CalcTextSize("Osc X").x);

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

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Amplitude Envelope");
    ImGui::PushItemWidth(slider_width);

    ADSR& amp_emv = ui_state.amp_env;

    // Attack slider
    render_slider("##attack", "Atk", &amp_emv.attack, 5.0f, true, "%.3f s");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) amp_emv.attack = 0.0f;

    // Decay slider
    ImGui::SameLine();
    render_slider("##decay", "Dky", &amp_emv.decay, 5.0f, true, "%.3f s");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) amp_emv.attack = 0.0f;

    // Sustain slider
    render_slider("##sustain", "Sus", &amp_emv.sustain, 1.0f, false, "%.3f");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) amp_emv.attack = 1.0f;

    // Release slider
    ImGui::SameLine();
    render_slider("##release", "Rls", &amp_emv.release, 5.0f, true, "%.3f s", 0.001f);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) amp_emv.attack = 0.001f;

    // Filter Envelope //

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Filter Envelope");
    ImGui::PushItemWidth(slider_width);

    ADSR& filt_env = ui_state.filt_env;

    // Attack slider
    render_slider("##filt-attack", "Atk", &filt_env.attack, 5.0f, true, "%.3f s");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) filt_env.attack = 0.0f;

    // Decay slider
    ImGui::SameLine();
    render_slider("##filt-decay", "Dky", &filt_env.decay, 5.0f, true, "%.3f s");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) filt_env.attack = 0.0f;

    // Sustain slider
    render_slider("##filt-sustain", "Sus", &filt_env.sustain, 1.0f, false, "%.3f");
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) filt_env.attack = 1.0f;

    // Release slider
    ImGui::SameLine();
    render_slider("##filt-release", "Rls", &filt_env.release, 5.0f, true, "%.3f s", 0.001f);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) filt_env.attack = 0.001f;

    // Filter Settings //
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Filter");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(combo_width - ImGui::CalcTextSize("Filter").x);
    if (ImGui::BeginCombo("##filter-type", FILTER_NAMES[ui_state.filter_type]))
    {
        for (int i = 0; i < 3; i++)
        {
            if (ImGui::Selectable(FILTER_NAMES[i], i == ui_state.filter_type)) ui_state.filter_type = (FilterType) i;

            if (i == ui_state.filter_type) {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Frq");
    ImGui::SameLine();
    ImGui::SliderFloat(
        "##filter-freq", &ui_state.filt_freq, 20.0f, modctx.sample_rate / 2.5f, "%.0f Hz",
        ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp
    );
    
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Res");
    ImGui::SameLine();
    ImGui::SliderFloat("##filter-q", &ui_state.filt_reso, -20.0f, 20.0f, "%.3f dB");
    
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Env");
    ImGui::SameLine();
    ImGui::SliderFloat("##filter-env", &ui_state.filt_amount, 0.0f, 1.0f);

    ImGui::EndGroup();

    ImGui::PopItemWidth();

    // send state to process thread
    state_queue.post(&ui_state, sizeof(ui_state));
}

void WaveformSynth::save_state(std::ostream& ostream) {
    // write version
    push_bytes<uint8_t>(ostream, 1);

    // write oscillator config
    for (size_t osc = 0; osc < 3; osc++)
    {
        push_bytes<uint8_t>(ostream, ui_state.waveform_types[osc]);
        push_bytes<float>(ostream, ui_state.volume[osc]);
        push_bytes<float>(ostream, ui_state.panning[osc]);
        push_bytes<int32_t>(ostream, ui_state.coarse[osc]);
        push_bytes<float>(ostream, ui_state.fine[osc]);
    }

    // write amp envelope params
    ADSR& amp_params = ui_state.amp_env;
    push_bytes<float>(ostream, amp_params.attack);
    push_bytes<float>(ostream, amp_params.decay);
    push_bytes<float>(ostream, amp_params.sustain);
    push_bytes<float>(ostream, amp_params.release);

    // write filter params
    push_bytes<uint8_t>(ostream, ui_state.filter_type);
    push_bytes<float>(ostream, ui_state.filt_freq);
    push_bytes<float>(ostream, ui_state.filt_reso);
    push_bytes<float>(ostream, ui_state.filt_amount);

    ADSR& filt_params = ui_state.filt_env;
    push_bytes<float>(ostream, filt_params.attack);
    push_bytes<float>(ostream, filt_params.decay);
    push_bytes<float>(ostream, filt_params.sustain);
    push_bytes<float>(ostream, filt_params.release);
}

bool WaveformSynth::load_state(std::istream& istream, size_t size)
{
    // get version
    uint8_t version = pull_bytesr<uint8_t>(istream);
    if (version > 1) return false; // invalid version

    // read oscillator config
    for (size_t osc = 0; osc < 3; osc++)
    {
        ui_state.waveform_types[osc] = static_cast<WaveformType>(pull_bytesr<uint8_t>(istream));
        ui_state.volume[osc] = pull_bytesr<float>(istream);
        ui_state.panning[osc] = pull_bytesr<float>(istream);
        ui_state.coarse[osc] = pull_bytesr<int32_t>(istream);
        ui_state.fine[osc] = pull_bytesr<float>(istream);
    }

    // read amplitude envelope config
    ui_state.amp_env.attack = pull_bytesr<float>(istream);
    ui_state.amp_env.decay = pull_bytesr<float>(istream);
    ui_state.amp_env.sustain = pull_bytesr<float>(istream);
    ui_state.amp_env.release = pull_bytesr<float>(istream);
    
    // read filter params
    if (version >= 1)
    {
        ui_state.filter_type = static_cast<FilterType>( pull_bytesr<uint8_t>(istream) );
        ui_state.filt_freq = pull_bytesr<float>(istream);
        ui_state.filt_reso = pull_bytesr<float>(istream);
        ui_state.filt_amount = pull_bytesr<float>(istream);

        ui_state.filt_env.attack = pull_bytesr<float>(istream);
        ui_state.filt_env.decay = pull_bytesr<float>(istream);
        ui_state.filt_env.sustain = pull_bytesr<float>(istream);
        ui_state.filt_env.release = pull_bytesr<float>(istream);
    }

    // version 0 has no filter params
    else
    {
        ui_state.filter_type = LowPassFilter;
        ui_state.filt_freq = modctx.sample_rate / 2.5f;
        ui_state.filt_reso = 0.0f;
        ui_state.filt_amount = 0.0f;
        ui_state.filt_env = ADSR();
    }

    state_queue.post(&ui_state, sizeof(ui_state));
    return true;
}