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

OmniSynth::module_state_t::module_state_t()
{
    global_volume = 1.0f;

    for (int osc = 0; osc < 3; osc++)
    {
        osc_shapes[osc] = 0.0f;
        osc_coarse[osc] = 0;
        osc_fine[osc] = 0.0f;
    }

    osc12_mix = 0.5f;
    osc12_volume = 1.0f;
    osc3_volume = 1.0f;

    sub_volume = 0.0f;
    sub_lower = false;
    sub_square = false;

    filter_type = FilterType::LowPass;
    filter_freq = 19200.0f;
    filter_reso = 0.0f;

    noise_volume = false;

    for (int i = 0; i < FX_MOD_TARGET_COUNT; i++)
    {
        fx_target_lfo1[i] = false;
        fx_target_lfo2[i] = false;
        fx_target_env[i] = false;
    }

    bitcrush = 0.0f;
    reverb_size = 0.61f;
    reverb_lowcut = 300.0f;
    reverb_mix = 0.0f;
    delay_time = 0.05f;
    delay_feedback = 0.8f;
    delay_mix = 0.0f;

    autopan_rate = 0.25f;
    autopan_amt = 0.0f;

    phaser_rate = 0.0f;
    phaser_mix = 0.0f;
    phaser_taps = 0.0f;
    phaser_notch = 0.0f;

    enable_seq = false;
    arp_or_seq = false;
    seq_steps = 8;
    seq_division = 0.25f;

    arp_octave = 1;
    arp_dir = ArpeggioDirection::Up;

    for (int i = 0; i < SEQUENCER_STEP_COUNT; i++)
    {
        seq_semitone[i] = 0;
        seq_note_on[i] = false;
    }
}

OmniSynth::Voice::Voice()
{
    phase[0] = phase[1] = phase[2] = 0.0f;
    last_sample[0] = last_sample[1] = last_sample[2] = 0.0f;
}

OmniSynth::Voice::Voice(int _key, float _freq, float _volume)
:   Voice()
{
    key = _key;
    freq = _freq;
    volume = _volume;
    active = true;
}

OmniSynth::OmniSynth(ModuleContext& modctx)
:   ModuleBase(true), modctx(modctx),
    event_queue(sizeof(NoteEvent), MAX_VOICES*2),
    state_queue(sizeof(module_state_t), 2)
{
    id = "synth.omnisynth";
    name = "Omnisynth";
    
    ui_state = module_state_t();
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
    if (ImGui::BeginTabBar("omnisynth-tabs"))
    {
        if (ImGui::BeginTabItem("Main"))
        {
            tab_main();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Env"))
        {
            tab_env();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("FX"))
        {
            tab_fx();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Seq"))
        {
            tab_seq();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

void OmniSynth::tab_main()
{
    constexpr int OSC_COUNT = 3;
    
    static float osc1_shape = 0.0f;
    static int osc1_coarse = 0;
    static float osc1_fine = 0.0f;
    static float osc2_shape = 0.0f;
    static int osc2_coarse = 0;
    static float osc2_fine = 0.0f;

    ImGui::PushItemWidth(ImGui::GetTextLineHeight() * 6.0f);

    ImGui::BeginGroup();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Osc 1");

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Wav");
    ImGui::SameLine();
    ImGui::SliderFloat("##osc1-shape", &osc1_shape, 0.0f, 1.0f);
    
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Crs");
    ImGui::SameLine();
    ImGui::SliderInt("##osc1-coarse", &osc1_coarse, -12, 12);
    
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Fne");
    ImGui::SameLine();
    ImGui::SliderFloat("##osc1-fine", &osc1_fine, -100.0f, 100.0f);
    
    ImGui::EndGroup();

    ImGui::PopItemWidth();
}

void OmniSynth::tab_env()
{

}

void OmniSynth::tab_fx()
{

}

void OmniSynth::tab_seq()
{

}

void OmniSynth::save_state(std::ostream& ostream) {
}

bool OmniSynth::load_state(std::istream& istream, size_t size)
{
    return false;
}