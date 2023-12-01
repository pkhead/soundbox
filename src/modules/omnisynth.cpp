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

    for (int osc = 0; osc < 2; osc++)
    {
        osc_shapes[osc] = 0.0f;
        osc_coarse[osc] = 0;
        osc_fine[osc] = 0.0f;
        osc_volume[osc] = 0.33f;
    }
    osc_mix = 0.0f;

    fm_vol = 0.0f;
    fm_mod = 0.0f;

    sub_volume = 0.0f;
    sub_lower = false;
    sub_square = false;

    filter_type = FilterType::LowPass;
    filter_freq = 19200.0f;
    filter_reso = 0.0f;

    noise_volume = false;
    glide = 0.0f;
    mono = false;

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
    ImGui::PushItemWidth(ImGui::GetTextLineHeight() * 6.0f);

    if (ImGui::BeginTabBar("omnisynth-tabs"))
    {
        if (ImGui::BeginTabItem("Main"))
        {
            tab_main();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Envelopes"))
        {
            tab_env();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Effects"))
        {
            tab_fx();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Sequencer/Appregiator"))
        {
            tab_seq();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
    
    ImGui::PopItemWidth();
}

void OmniSynth::tab_main()
{
    module_state_t& state = ui_state;

    for (int osc = 0; osc < 2; osc++)
    {
        if (osc > 0) ImGui::SameLine();

        ImGui::PushID(osc);

        ImGui::BeginGroup();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Osc %i", osc+1);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Wav");
        ImGui::SameLine();
        ImGui::SliderFloat("##osc1-shape", &state.osc_shapes[osc], 0.0f, 1.0f);
        
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Crs");
        ImGui::SameLine();
        ImGui::SliderInt("##osc1-coarse", &state.osc_coarse[osc], -12, 12);
        
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Fne");
        ImGui::SameLine();
        ImGui::SliderFloat("##osc1-fine", &state.osc_fine[osc], -100.0f, 100.0f);
        
        ImGui::EndGroup();

        ImGui::PopID();
    }

    // fm oscillator
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("FM");

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Vol");
    ImGui::SameLine();
    ImGui::SliderFloat("##fm-volume", &state.fm_vol, 0.0f, 1.0f);
    
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Mod");
    ImGui::SameLine();
    ImGui::SliderFloat("##fm-mod", &state.fm_mod, 0.0f, 1.0f);
    
    ImGui::EndGroup();

    // sub oscillator
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Sub");

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Vol");
    ImGui::SameLine();
    ImGui::SliderFloat("##sub-volume", &state.sub_volume, 0.0f, 1.0f);
    
    ImGui::AlignTextToFramePadding();
    ImGui::Text("-24");
    ImGui::SameLine();
    ImGui::Checkbox("##sub-lower", &state.sub_lower);

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Sqr");
    ImGui::SameLine();
    ImGui::Checkbox("##sub-square", &state.sub_square);
    
    ImGui::EndGroup();

    // NEW LINE

    // osc volume & mix
    ImGui::BeginGroup();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Volume");

    for (int osc = 0; osc < 2; osc++)
    {
        ImGui::PushID(osc);
        
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Osc%i", osc+1);
        ImGui::SameLine();
        ImGui::SliderFloat("##osc-vol", &state.osc_volume[osc], 0.0f, 1.0f);

        ImGui::PopID();
    }

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Mix ");
    ImGui::SameLine();
    ImGui::SliderFloat("##12-mix", &state.osc_mix, -1.0f, 1.0f);
    ImGui::EndGroup();

    // filter
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Filter");

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Type");
    ImGui::SameLine();

    // filter type
    static const char* FILTER_TYPES[] = {
        "Low Pass", "High Pass", "Band Pass"
    };

    int filter_type = state.filter_type;
    if (ImGui::BeginCombo("##noise-type", FILTER_TYPES[filter_type]))
    {
        for (int i = 0; i < 3; i++)
        {
            bool selected = filter_type == i;
            if (ImGui::Selectable(FILTER_TYPES[i], selected))
                filter_type = i;

            if (selected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }
    state.filter_type = static_cast<FilterType>(filter_type);

    // filter config
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Freq");
    ImGui::SameLine();
    ImGui::SliderFloat("##filter-freq", &state.filter_freq, 20.0f, modctx.sample_rate / 2.5f);

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Reso");
    ImGui::SameLine();
    ImGui::SliderFloat("##filter-reso", &state.filter_reso, 0.0f, 10.0f);

    ImGui::EndGroup();

    // more options
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Other");

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Noise");
    ImGui::SameLine();
    ImGui::SliderFloat("##noise-vol", &state.noise_volume, 0.0f, 1.0f);

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Glide");
    ImGui::SameLine();
    ImGui::SliderFloat("##glide", &state.glide, 0.0f, 0.15f);

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Mono ");
    ImGui::SameLine();
    ImGui::Checkbox("##mono", &state.mono);

    ImGui::EndGroup();
}

void OmniSynth::tab_env()
{

}

void OmniSynth::tab_fx()
{
    module_state_t& state = ui_state;

    static const char* TARGET_NAMES[] = {
        "Cutoff",
        "Reso",
        "Osc Mix",
        "Reverb",
        "Decay",
        "Noise",
        "FM Mod",
        "Filter Env",
        "Pitch",
        "Bitcrush",
        "Tremolo",
    };

    // mod route labels
    ImGui::BeginGroup();
    for (int i = 0; i < FX_MOD_TARGET_COUNT; i++)
    {
        ImGui::Text("%s", TARGET_NAMES[i]);
    }
    ImGui::EndGroup();

    // mod route buttons
    ImGui::SameLine();
    ImGui::BeginGroup();
    for (int i = 0; i < FX_MOD_TARGET_COUNT; i++)
    {
        ImGui::SmallButton("1");
        ImGui::SameLine();
        ImGui::SmallButton("2");
        ImGui::SameLine();
        ImGui::SmallButton("E");
    }
    ImGui::EndGroup();

    ImGui::SameLine();
    ImGui::BeginGroup();

    // lfos
    for (int i = 0; i < 2; i++)
    {
        if (i > 0) ImGui::SameLine();

        ImGui::PushID(i);
        ImGui::BeginGroup();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("LFO %i", i+1);

        int shape = state.lfo_shape[i];

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Shp");
        ImGui::SameLine();
        ImGui::SliderInt("##lfo-shape", &shape, 0, 4); // TODO: combobox
        state.lfo_shape[i] = static_cast<LFOShape>(shape);
        
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Frq");
        ImGui::SameLine();
        ImGui::SliderFloat("##lfo-rate", &state.lfo_rate[i], 0.0f, 80.0f);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Amt");
        ImGui::SameLine();
        ImGui::SliderFloat("##lfo-rate", &state.lfo_amp[i], 0.0f, 80.0f);

        ImGui::PopID();
        ImGui::EndGroup();
    }

    ImGui::EndGroup();
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