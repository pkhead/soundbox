#include <math.h>
#include <cstdint>
#include <cstdlib>
#include <imgui.h>
#include "../sys.h"
#include "../song.h"
#include "../util.h"
#include "omnisynth.h"

using namespace audiomod;

static double _mod(double a, double b) {
    return fmod(fmod(a, b) + b, b);
}

static constexpr double PI = M_PI;
static constexpr double PI2 = 2.0f * PI;

OmniSynth::OmniSynth(ModuleContext& modctx)
:   ModuleBase(true), modctx(modctx),
    event_queue(sizeof(NoteEvent), MAX_VOICES*2),
    state_queue(sizeof(module_state_t), 2)
{
    id = "synth.omnisynth";
    name = "Omnisynth";
    
    // set up initial state
    for (int i = 0; i < 6; i++)
    {
        ui_state.waveforms[i] = Sine;
        ui_state.volume[i] = 0.0f;
        ui_state.freq[i] = 1.0f;
    }
    ui_state.volume[0] = 1.0f;

    ui_state.amp_envelope.attack = 0.0f;
    ui_state.amp_envelope.decay = 0.0f;
    ui_state.amp_envelope.sustain = 1.0f;
    ui_state.amp_envelope.release = 0.0f;

    ui_state.filter_envelope = ui_state.amp_envelope;
    ui_state.filter_envelope_amt = 0.0f;
    
    ui_state.filter_type = LowPass;
    ui_state.filter_freq = modctx.sample_rate / 2.5f;
    ui_state.filter_reso = 0.0f;

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

void OmniSynth::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) {
    // first, get state
    while (true)
    {
        auto handle = state_queue.read();
        if (handle.size() == 0) break;

        assert(handle.size() == sizeof(module_state_t));
        handle.read(&process_state, sizeof(module_state_t));
    }

    // then, read queued note events
    while (true) {
        auto handle = event_queue.read();
        if (!handle) break;

        assert(handle.size() == sizeof(NoteEvent));
        NoteEvent ev;
        handle.read(&ev, sizeof(NoteEvent));
        event(ev);
    }

    memset(output, 0, buffer_size * sizeof(float));
}

void OmniSynth::queue_event(const NoteEvent& event)
{
    event_queue.post(&event, sizeof(event));
}

void OmniSynth::event(const NoteEvent& event) {
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
            voice->active = true;
            voice->key = event.key;
            voice->freq = key_freq;
            voice->volume = event.volume;
            voice->time = 0.0f;
            voice->release_time = -1.0f;
            voice->release_env = 0.0f;

            for (int i = 0; i < 6; i++)
            {
                voice->phase[i] = 0.0f;
                voice->last_sample[i] = 0.0f;
            }
        }
    
    } else if (event.kind == NoteEventKind::NoteOff) {
        for (size_t i = 0; i < MAX_VOICES; i++) {
            Voice& voice = voices[i];

            if (voice.active && voice.key == event.key && voice.release_time < 0.0f) {
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

static void render_slider(const char* id, const char* label, float* var, float max, bool log, const char* fmt, float start = 0.0f) {
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s", label);
    ImGui::SameLine();
    ImGui::SliderFloat(
        id, var, start, max, fmt,
        (log ? ImGuiSliderFlags_Logarithmic : 0) | ImGuiSliderFlags_NoRoundToFormat
    );
}

void OmniSynth::_interface_proc() {
    static const char* WAVEFORM_NAMES[] = {
        "Sine",
        "Square",
        "Sawtooth",
        "Triangle",
        "Pulse",
        "Noise",
    };

    ImGuiStyle& style = ImGui::GetStyle();
    
    const float vslider_width = ImGui::GetFrameHeight();
    const float vslider_height = ImGui::GetTextLineHeight() * 8;

    ImGui::AlignTextToFramePadding();
    ImGui::BeginGroup();

    ImGui::Text("Operators");

    for (int osc = 0; osc < 6; osc++) {
        ImGui::PushID(osc);

        //ImGui::PushItemWidth(slider_width);
        if (osc > 0)
            ImGui::SameLine();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x / 2.0f, style.ItemSpacing.y));
        
        ImGui::VSliderFloat("##freq", ImVec2(vslider_width, vslider_height), ui_state.freq+osc, 0.25f, 20.0f, "F");
        ui_state.freq[osc] = (int)(ui_state.freq[osc] + 0.5f);
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            ImGui::SetTooltip("Frequency: %.3fx", ui_state.freq[osc]);

        ImGui::SameLine();
        ImGui::VSliderFloat("##amp", ImVec2(vslider_width, vslider_height), ui_state.volume+osc, 0.0f, 1.0f, "A");
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            ImGui::SetTooltip("Amplitude: %.3f", ui_state.volume[osc]);

        ImGui::PopStyleVar();
        
        ImGui::PopID();
    }

    ImGui::EndGroup();
    ImGui::SameLine();

    ImGui::BeginGroup();

    // AMPLITUDE ENVELOPE //
    ImGui::Text("Amplitude Envelope");
    {
        ImGui::PushID("ampenv");

        ImGui::PushItemWidth(ImGui::CalcTextSize("A: 9.999 s").x);
        
        ImGui::SliderFloat("##attack", &ui_state.amp_envelope.attack, 0.0f, 10.0f, "A: %.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SameLine();
        ImGui::SliderFloat("##decay", &ui_state.amp_envelope.decay, 0.0f, 10.0f, "D: %.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("##sustain", &ui_state.amp_envelope.sustain, 0.0f, 1.0f, "S: %.3f");
        ImGui::SameLine();
        ImGui::SliderFloat("##release", &ui_state.amp_envelope.release, 0.0f, 10.0f, "R: %.3f", ImGuiSliderFlags_Logarithmic);
    
        ImGui::PopID();
    }

    // FILTER ENVELOPE //
    ImGui::Text("Filter Envelope");
    {
        ImGui::PushID("filtenv");

        ImGui::PushItemWidth(ImGui::CalcTextSize("A: 9.999 s").x);
        
        ImGui::BeginGroup();
        ImGui::SliderFloat("##attack", &ui_state.filter_envelope.attack, 0.0f, 10.0f, "A: %.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SameLine();
        ImGui::SliderFloat("##decay", &ui_state.filter_envelope.decay, 0.0f, 10.0f, "D: %.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("##sustain", &ui_state.filter_envelope.sustain, 0.0f, 1.0f, "S: %.3f");
        ImGui::SameLine();
        ImGui::SliderFloat("##release", &ui_state.filter_envelope.release, 0.0f, 10.0f, "R: %.3f", ImGuiSliderFlags_Logarithmic);
        ImGui::EndGroup();

        ImGui::SameLine();

        float vslider_w = ImGui::GetFrameHeight();
        float vslider_h = ImGui::GetFrameHeightWithSpacing() + ImGui::GetFrameHeight();
        ImGui::VSliderFloat("##filter-amount",
            ImVec2(vslider_w, vslider_h), &ui_state.filter_envelope_amt, 0.0f, 1.0f,
            "", ImGuiSliderFlags_NoRoundToFormat | ImGuiSliderFlags_AlwaysClamp);

        ImGui::PopItemWidth();
        ImGui::PopID();
    }

    ImGui::EndGroup();
}

void OmniSynth::save_state(std::ostream& ostream) {
}

bool OmniSynth::load_state(std::istream& istream, size_t size)
{
    return false;
}