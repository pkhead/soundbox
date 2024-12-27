/**
* Extra GUI widgets for DSP
**/
#pragma once
#include <string>
#include <imguiext/imgui-knobs.h>

namespace widgets
{
    bool knob(
        const char *label,
        float *p_value,
        float v_min,
        float v_max,
        const char *fmt = "%.3f",
        ImGuiKnobFlags flags = 0,
        float speed = 0.0f,
        ImGuiKnobVariant variant = ImGuiKnobVariant_Dot,
        float size = 0,
        int steps = 10
    );

    bool knob_int(
        const char *label,
        int *p_value,
        int v_min,
        int v_max,
        const char *fmt = "%.3f",
        ImGuiKnobFlags flags = 0,
        float speed = 0.0f,
        ImGuiKnobVariant variant = ImGuiKnobVariant_Dot,
        float size = 0,
        int steps = 10
    );

    struct adsr_ui_struct
    {
        float attack;
        float decay;
        float sustain;
        float release;

        float attack_max;
        float decay_max;
        float release_max;

        adsr_ui_struct(float a, float d, float s, float r);
    };

    bool adsr_ui(const std::string &id, ImVec2 size, adsr_ui_struct *data);

    void horiz_vu_meter(float level, float peak, float max_db = 0.0f);
}