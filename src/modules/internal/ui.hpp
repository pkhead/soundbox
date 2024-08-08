#pragma once
#include <string>
#include <imgui.h>

namespace hosts::internal::ui
{
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
}