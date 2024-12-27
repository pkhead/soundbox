#include <cmath>
#include <math.h>
#include <util.hpp>
#include <widgets.hpp>
#include <imgui.h>
#include <log.hpp>
#include <imguiext/imgui-knobs.h>
#include <module_hosts/modules.hpp>
#include "waveform.hpp"

using namespace hosts::internal;

WaveformModule::Voice::Voice()
{
    phase[0] = phase[1] = phase[2] = 0.0f;
    last_sample[0] = last_sample[1] = last_sample[2] = 0.0f;
    filter_freq = -1.0f;
}

WaveformModule::Voice::Voice(int _key, float _freq, float _volume) :
    Voice()
{
    key = _key;
    freq = _freq;
    volume = _volume;
    active = true;
}

WaveformModule::WaveformModule(modules::ModuleCreator &create) :
    modx::ModuleBase(create),
    event_reader(0)
{
    // setup module i/o and controls    
    create.add_message_input();
    create.add_audio_output(2);

    for (unsigned int i = 0; i < OSC_COUNT; i++)
    {
        char buf[64];
        unsigned int control_start = OSC_CONTROL_START[i];

        // osc type
        sprintf(buf, "Oscillator %i Type", i);
        create.add_control<int>(control_start + CONTROL_OSC_TYPE, buf, WAVE_SINE);

        // osc vol
        sprintf(buf, "Oscillator %i Volume", i);
        create.add_control<float>(control_start + CONTROL_OSC_VOL, buf, i == 0 ? 0.5f : 0.0f);

        // osc pan
        sprintf(buf, "Oscillator %i Panning", i);
        create.add_control<float>(control_start + CONTROL_OSC_PAN, buf, 0.0f);

        // osc coarse
        sprintf(buf, "Oscillator %i Coarse Detune", i);
        create.add_control<int>(control_start + CONTROL_OSC_COARSE, buf, 0);

        // osc fine
        sprintf(buf, "Oscillator %i Fine Detune", i);
        create.add_control<float>(control_start + CONTROL_OSC_FINE, buf, 0.0f);
    }
    
    create.add_control<float>(CONTROL_AMP_ATTACK, "Amplitude Envelope Attack", 0.0f);
    create.add_control<float>(CONTROL_AMP_SUSTAIN, "Amplitude Envelope Sustain", 1.0f);
    create.add_control<float>(CONTROL_AMP_DECAY, "Amplitude Envelope Decay", 0.0f);
    create.add_control<float>(CONTROL_AMP_RELEASE, "Amplitude Envelope Release", 0.0f);

    create.add_control<float>(CONTROL_FILTER_ATTACK, "Filter Envelope Attack", 0.0f);
    create.add_control<float>(CONTROL_FILTER_SUSTAIN, "Filter Envelope Sustain", 1.0f);
    create.add_control<float>(CONTROL_FILTER_DECAY, "Filter Envelope Decay", 0.0f);
    create.add_control<float>(CONTROL_FILTER_RELEASE, "Filter Envelope Release", 0.0f);

    create.add_control<int>(CONTROL_FILTER_TYPE, "Filter Type", FILTER_LOW_PASS);
    create.add_control<float>(CONTROL_FILTER_FREQ, "Filter Frequency", (float)create.engine.sample_rate() * 0.35f);
    create.add_control<float>(CONTROL_FILTER_RESO, "Filter Resonance", 1.0f);
    create.add_control<float>(CONTROL_FILTER_ENV, "Filter Envelope", 0.0f);

    create.add_control<float>(CONTROL_VIBRATO_DELAY, "Vibrato Delay", 0.0f);
    create.add_control<float>(CONTROL_VIBRATO_SPEED, "Vibrato Speed", 2.0f);
    create.add_control<float>(CONTROL_VIBRATO_AMOUNT, "Vibrato Amount", 0.0f);

    // setup internal module state
    for (int i = 0; i < MAX_VOICES; i++)
    {
        audio_state.voices[i] = {};
        audio_state.voices[i].active = false;
    }

    ui_selected_osc = 1;
}

// https://www.martin-finke.de/articles/audio-plugins-018-polyblep-oscillator/
static float poly_blep(float t, float inc)
{
    float dt = inc / (2.0f * dsp::PIf);
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

void WaveformModule::event(const modx::TrackEvent& ev) {
    if (ev.event_kind == modx::TrackEvent::NOTE_ON) {
        // create new voice in first found empty slot
        // if there are no empty slots, replace the first voice in memory
        Voice* voice = audio_state.voices+0;

        for (size_t i = 0; i < MAX_VOICES; i++)
        {
            if (!audio_state.voices[i].active)
            {
                voice = audio_state.voices+i;
                break;
            }
        }

        float key_freq = powf(2.0f, (float)(ev.note.key - 57) / 12.0f) * 440.0f;;
        *voice = Voice(ev.note.key, key_freq, (float)ev.note.velocity / 127.0f);
    
    } else if (ev.event_kind == modx::TrackEvent::NOTE_OFF) {
        for (size_t i = 0; i < MAX_VOICES; i++) {
            Voice& voice = audio_state.voices[i];

            if (voice.active && voice.key == ev.note.key && !voice.amp_env.is_released()) {
                voice.amp_env.release(audio_state.amp_env);
                voice.filt_env.release(audio_state.filt_env);
                break;
            }
        }
    }
}

void WaveformModule::process(modules::ModuleProcessor &proc)
{    
    dsp::ADSR &amp_env_params = audio_state.amp_env;
    dsp::ADSR &filt_env_params = audio_state.filt_env;

    // read control values //
    amp_env_params.attack = proc.get_control_value<float>(CONTROL_AMP_ATTACK);
    amp_env_params.decay = proc.get_control_value<float>(CONTROL_AMP_DECAY);
    amp_env_params.sustain = proc.get_control_value<float>(CONTROL_AMP_SUSTAIN);
    amp_env_params.release = proc.get_control_value<float>(CONTROL_AMP_RELEASE);
    
    filt_env_params.attack = proc.get_control_value<float>(CONTROL_FILTER_ATTACK);
    filt_env_params.decay = proc.get_control_value<float>(CONTROL_FILTER_DECAY);
    filt_env_params.sustain = proc.get_control_value<float>(CONTROL_FILTER_SUSTAIN);
    filt_env_params.release = proc.get_control_value<float>(CONTROL_FILTER_RELEASE);

    int filter_type = proc.get_control_value<int>(CONTROL_FILTER_TYPE);
    float ctl_filter_freq = proc.get_control_value<float>(CONTROL_FILTER_FREQ);
    float filt_amount = proc.get_control_value<float>(CONTROL_FILTER_ENV);
    float reso_linear = dsp::db_to_mult(proc.get_control_value<float>(CONTROL_FILTER_RESO));

    float vibrato_amount = proc.get_control_value<float>(CONTROL_VIBRATO_AMOUNT);
    float vibrato_delay = proc.get_control_value<float>(CONTROL_VIBRATO_DELAY);
    float vibrato_speed = proc.get_control_value<float>(CONTROL_VIBRATO_SPEED);

    struct
    {
        int type;
        float vol, pan;
        int coarse;
        float fine;
    } osc_data[OSC_COUNT];

    for (int i = 0; i < OSC_COUNT; i++)
    {
        osc_data[i].type = proc.get_control_value<int>(OSC_CONTROL_START[i] + CONTROL_OSC_TYPE);
        osc_data[i].vol = proc.get_control_value<float>(OSC_CONTROL_START[i] + CONTROL_OSC_VOL);
        osc_data[i].pan = proc.get_control_value<float>(OSC_CONTROL_START[i] + CONTROL_OSC_PAN);
        osc_data[i].coarse = proc.get_control_value<int>(OSC_CONTROL_START[i] + CONTROL_OSC_COARSE);
        osc_data[i].fine = proc.get_control_value<float>(OSC_CONTROL_START[i] + CONTROL_OSC_FINE);
    }
    ////////////////////

    if (amp_env_params.release < 0.001f) amp_env_params.release = 0.001f;
    if (filt_env_params.release < 0.001f) filt_env_params.release = 0.001f;

    float samples[3][2];

    float *output = proc.audio_output(0);
    int channel_count = proc.audio_output_channels(0);

    event_reader.new_run(&proc);

    for (size_t i = 0; i < proc.buffer_frame_count * channel_count; i += channel_count) {
        modx::TrackEvent event;
        while (event_reader.read(&event))
            this->event(event);

        // set both channels to zero
        for (size_t ch = 0; ch < channel_count; ch++) output[i + ch] = 0.0f;

        // compute all voices
        for (size_t j = 0; j < MAX_VOICES; j++) {
            Voice& voice = audio_state.voices[j];
            if (!voice.active) continue;

            float amp_env;
            if (voice.amp_env.compute(proc.sample_rate, amp_env, amp_env_params))
            {
                // note ended
                voice.active = false;
                continue;
            }

            float filt_env;
            voice.filt_env.compute(proc.sample_rate, filt_env, filt_env_params);

            // setup filter
            float filt_env_min = 5.0f; // going too low on frequency will do... Something
            float filt_freq = util::lerp(ctl_filter_freq, util::lerp(filt_env_min, ctl_filter_freq, filt_env), filt_amount);

            // move filter frequency to target
            /*{
                constexpr float HZ_PER_SEC = 150000.f;

                if (voice.filter_freq < 0.0f) {
                    voice.filter_freq = filt_freq;
                } else {
                    float hzSpeed = HZ_PER_SEC / proc.sample_rate;
                    float delta = filt_freq - voice.filter_freq;
                    if (fabsf(delta) < hzSpeed) {
                        voice.filter_freq = filt_freq;
                    } else {
                        voice.filter_freq += hzSpeed * util::bsign(delta);
                    }
                }
            }*/
            voice.filter_freq = filt_freq;

            switch (filter_type)
            {
                case FILTER_LOW_PASS:
                    voice.filter[0].low_pass(proc.sample_rate, voice.filter_freq, reso_linear);
                    voice.filter[1].low_pass(proc.sample_rate, voice.filter_freq, reso_linear);
                    break;

                case FILTER_HIGH_PASS:
                    voice.filter[0].high_pass(proc.sample_rate, voice.filter_freq, reso_linear);
                    voice.filter[1].high_pass(proc.sample_rate, voice.filter_freq, reso_linear);
                    break;

                /*case BandPassFilter:
                    // TODO: Band pass filter
                    break;*/
            }

            float vibrato_amt = sinf(voice.vibrato_phase) * vibrato_amount;

            for (size_t osc = 0; osc < OSC_COUNT; osc++) {
                float freq = voice.freq * powf(2.0f, ((float)osc_data[osc].coarse + (osc_data[osc].fine + vibrato_amt) / 100.0f) / 12.0f);
                double phase = voice.phase[osc];
                double period = 1.0 / freq;
                float sample;

                float r_mult = (osc_data[osc].pan + 1.0f) / 2.0f;
                float l_mult = 1.0f - r_mult;

                double increment = (2.0f * dsp::PIf * freq) / proc.sample_rate;

                switch (osc_data[osc].type) {
                    case WAVE_SINE:
                        sample = sinf(phase);
                        break;

                    case WAVE_SQUARE:
                    case WAVE_TRIANGLE: // triangle is an integrated square wave
                        sample = phase < dsp::PIf ? 1.0 : -1.0;
                        sample += poly_blep(phase / (2.0f * dsp::PIf), increment);
                        sample -= poly_blep(fmod(phase / (2.0f * dsp::PIf) + 0.5,1.0), increment);

                        if (osc_data[osc].type == WAVE_TRIANGLE)
                        {
                            // this doesn't quite make an accurate triangle wave,
                            // though it sounds enough like one. just in case, i'm
                            // keeping the code to make an actual triangle wave, just
                            // commented out
                            sample = increment * sample + (1 - increment * 0.25f) * voice.last_sample[osc];
                            voice.last_sample[osc] = sample;
                        }

                        break;
                    
                    /*case Triangle: {
                        sample = -1.0 + (2.0 * phase / PI2);
                        sample = 2.0 * (fabs(sample) - 0.5);
                        break;
                    }*/

                    case WAVE_SAWTOOTH:
                        sample = (2.0 * phase / (2.0f * dsp::PIf)) - 1.0;
                        sample -= poly_blep(phase / (2.0f * dsp::PIf), increment);
                        break;

                    // 25% pulse wave
                    case WAVE_PULSE: {
                        // phase-shift
                        double mphase = util::mod((phase + dsp::PI/2.0), 2*dsp::PIf);

                        double a = phase / dsp::PIf - 1.0;
                        a -= poly_blep(phase / (2*dsp::PIf), increment);
                        double b = mphase / dsp::PIf - 1.0;
                        b -= poly_blep(mphase / (2*dsp::PIf), increment);
                        sample = a - b;
                        break;
                    }

                    // a pulse wave: value[w] = (2.0f * _modf(phase / M_2PI + 0.5f, 1.3f) - 1.0f) > 0.0f ? 1.0f : -1.0f;
                }

                sample *= amp_env * voice.volume * osc_data[osc].vol;
                samples[osc][0] = sample * l_mult;
                samples[osc][1] = sample * r_mult;

                voice.phase[osc] += increment;
                if (voice.phase[osc] > 2*dsp::PI)
                    voice.phase[osc] -= 2*dsp::PI;
            }

            // update vibrato
            if (voice.time >= vibrato_delay)
            {
                voice.vibrato_phase += (2*dsp::PIf * vibrato_speed) / proc.sample_rate;
                
                if (voice.vibrato_phase > 2*dsp::PIf) {
                    voice.vibrato_phase -= 2*dsp::PIf;
                }
            }

            // apply filter
            float voice_l = samples[0][0] + samples[1][0] + samples[2][0];
            float voice_r = samples[0][1] + samples[1][1] + samples[2][1];
            voice.filter[0].process(&voice_l);
            voice.filter[1].process(&voice_r);

            output[i] += voice_l;
            output[i + 1] += voice_r;

            voice.time += 1.0 / proc.sample_rate;
        }
    }
}

void WaveformModule::ui()
{
    float knob_size = ImGui::GetFontSize() * 3.0f;
    ImGuiKnobFlags knob_flags = 0;
    ImVec2 frame_padding_thin = ImVec2(ImGui::GetStyle().FramePadding.x, 0.0f);

    ImGuiChildFlags group_child_flags = ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_Border;
    {
        int i = ui_selected_osc - 1;

        ImGui::BeginChild("osc", ImVec2(0.0f, -FLT_MIN), group_child_flags);

        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderInt("##SelectedOsc", &ui_selected_osc, 1, 3, "%d", ImGuiSliderFlags_AlwaysClamp);
        
        ImGui::TextDisabled("Oscillator %i", i+1);

        // waveform combobox
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, frame_padding_thin);
        int waveform = engine->control_get_value<int>(id(), OSC_CONTROL_START[i] + CONTROL_OSC_TYPE);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Waveform");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8.0f);
        if (ImGui::Combo("##waveform", &waveform, "Square\0Sawtooth\0Pulse\0Triangle\0Sine\0"))
            engine->control_set_value<int>(id(), OSC_CONTROL_START[i] + CONTROL_OSC_TYPE, waveform);
        ImGui::PopStyleVar();

        // control knobs
        float vol = get_control_value<float>(OSC_CONTROL_START[i] + CONTROL_OSC_VOL) * 100.0f;
        if (widgets::knob("Vol", &vol, 0.0f, 100.0f, "%.0f"))
            set_control_value<float>(OSC_CONTROL_START[i] + CONTROL_OSC_VOL, vol / 100.0f);
        
        ImGui::SameLine();
        ui_knob("Pan", OSC_CONTROL_START[i] + CONTROL_OSC_PAN, -1.0f, 1.0f, "%.2f");
        ImGui::SameLine();
        ui_knob_int("Crs", OSC_CONTROL_START[i] + CONTROL_OSC_COARSE, -24, 24, "%i st");
        ImGui::SameLine();
        ui_knob("Fine", OSC_CONTROL_START[i] + CONTROL_OSC_FINE, -100.0f, 100.0f, "%.0fc");
        ImGui::EndChild();
    }

    // filter options
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::BeginChild("filter", ImVec2(0.0f, 0.0f), group_child_flags | ImGuiChildFlags_AutoResizeY);
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 0.0f));
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Filter");

        // filter type combobox
        int filter_type = engine->control_get_value<int>(id(), CONTROL_FILTER_TYPE);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8.0f);
        if (ImGui::Combo("##filter_type", &filter_type, "Low Pass\0High Pass\0"))
            engine->control_set_value<int>(id(), CONTROL_FILTER_TYPE, filter_type);
        ImGui::PopStyleVar();

        ui_knob("Freq", CONTROL_FILTER_FREQ, 64.0f, engine->sample_rate() * 0.4f, "%.3f", ImGuiKnobFlags_Logarithmic);
        ImGui::SameLine();
        ui_knob("Reso", CONTROL_FILTER_RESO, 0.0f, 16.0f, "%.3f dB");
        ImGui::SameLine();
        ui_knob("Env", CONTROL_FILTER_ENV, 0.0f, 1.0f, "%.2f");
    }
    ImGui::EndChild();

    // vibrato options
    ImGui::SameLine();
    ImGui::BeginChild("vibrato", ImVec2(0.0f, 0.0f), group_child_flags | ImGuiChildFlags_AutoResizeY);
    {
        ImGui::TextDisabled("Vibrato");

        ImGuiKnobFlags flags = ImGuiKnobFlags_NoInput;
        ui_knob("Dly", CONTROL_VIBRATO_DELAY, 0.0f, 2.0f, "%.3f", flags);
        ImGui::SameLine();
        ui_knob("Spd", CONTROL_VIBRATO_SPEED, 0.0f, 20.0f, "%.3f", flags);
        ImGui::SameLine();
        ui_knob("Amt", CONTROL_VIBRATO_AMOUNT, -100.0f, 100.0f, "%.0fc");
    }
    ImGui::EndChild();

    // amplitude envelope
    {
        widgets::adsr_ui_struct adsr_struct(
            get_control_value<float>(CONTROL_AMP_ATTACK),
            get_control_value<float>(CONTROL_AMP_DECAY),
            get_control_value<float>(CONTROL_AMP_SUSTAIN),
            get_control_value<float>(CONTROL_AMP_RELEASE)
        );

        if (widgets::adsr_ui("Amplitude Envelope", ImVec2(ImGui::GetFontSize() * 12.0f, 0.0f), &adsr_struct))
        {
            set_control_value<float>(CONTROL_AMP_ATTACK, adsr_struct.attack);
            set_control_value<float>(CONTROL_AMP_DECAY, adsr_struct.decay);
            set_control_value<float>(CONTROL_AMP_SUSTAIN, adsr_struct.sustain);
            set_control_value<float>(CONTROL_AMP_RELEASE, adsr_struct.release);
        }
    }

    ImGui::SameLine();
    // filter envelope
    {
        widgets::adsr_ui_struct adsr_struct(
            get_control_value<float>(CONTROL_FILTER_ATTACK),
            get_control_value<float>(CONTROL_FILTER_DECAY),
            get_control_value<float>(CONTROL_FILTER_SUSTAIN),
            get_control_value<float>(CONTROL_FILTER_RELEASE)
        );

        if (widgets::adsr_ui("Filter Envelope", ImVec2(ImGui::GetFontSize() * 12.0f, 0.0f), &adsr_struct))
        {
            set_control_value<float>(CONTROL_FILTER_ATTACK, adsr_struct.attack);
            set_control_value<float>(CONTROL_FILTER_DECAY, adsr_struct.decay);
            set_control_value<float>(CONTROL_FILTER_SUSTAIN, adsr_struct.sustain);
            set_control_value<float>(CONTROL_FILTER_RELEASE, adsr_struct.release);
        }
    }

    /*ImGui::BeginChild("ampenv", ImVec2(-FLT_MIN, 0.0f), group_child_flags);
    ImGui::TextDisabled("Amplitude Envelope");
    ui_knob("Atk", CONTROL_AMP_ATTACK, 0.0f, 10.0f, 0.0f, "%.2f s");
    ImGui::SameLine();
    ui_knob("Dky", CONTROL_AMP_DECAY, 0.0f, 10.0f, 0.0f, "%.2f s");
    ImGui::SameLine();
    ui_knob("Sus", CONTROL_AMP_SUSTAIN, 0.0f, 1.0f, 0.0f, "%.2f s");
    ImGui::SameLine();
    ui_knob("Rls", CONTROL_AMP_RELEASE, 0.0f, 10.0f, 0.0f, "%.2f s");
    ImGui::EndChild();*/
    ImGui::EndGroup();
}    

