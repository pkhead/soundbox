#include "widgets.hpp"

#include <algorithm>
#include <cfloat>
#include <imgui.h>
#include <math.h>
#include <util.hpp>
#include "imgui_internal.h"
#include "widgets.hpp"

bool widgets::knob(
    const char *label,
    float *p_value,
    float v_min,
    float v_max,
    const char *fmt,
    ImGuiKnobFlags flags,
    float speed,
    ImGuiKnobVariant variant,
    float size ,
    int steps
)
{
    if (size == 0.0f)
    {
        size = ImGui::GetFontSize() * 2.5f;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 0.0f));
    bool changed = ImGuiKnobs::Knob(label, p_value, v_min, v_max, speed, fmt, variant, size, flags, steps);
    ImGui::PopStyleVar();
    return changed;
}

bool widgets::knob_int(
    const char *label,
    int *p_value,
    int v_min,
    int v_max,
    const char *fmt,
    ImGuiKnobFlags flags,
    float speed,
    ImGuiKnobVariant variant,
    float size,
    int steps
)
{
    if (size == 0.0f)
    {
        size = ImGui::GetFontSize() * 2.5f;
    }
    
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 0.0f));
    bool changed = ImGuiKnobs::KnobInt(label, p_value, v_min, v_max, speed, fmt, variant, size, flags, steps);
    ImGui::PopStyleVar();
    return changed;
}

widgets::adsr_ui_struct::adsr_ui_struct(float a, float d, float s, float r)
{
    attack = a;
    decay = d;
    sustain = s;
    release = r;

    attack_max = 5.0f;
    decay_max = 5.0f;
    release_max = 5.0f;
}

float to_parametric_log(float v, float min, float max)
{
    assert(min >= 0.0f && max >= 0.0f);
    assert(max > min);

    constexpr float epsilon = 0.002f;
    min = util::max(epsilon, min);
    max = util::max(epsilon, max);

    if (v <= epsilon) return 0.0f;
    if (v >= max) return 1.0f;
    return logf(v / min) / logf(max / min);
}

float from_parametric_log(float t, float min, float max)
{
    assert(min >= 0.0f && max >= 0.0f);
    assert(max > min);

    if (t <= 0.0f) return min;
    if (t >= 1.0f) return max;

    constexpr float epsilon = 0.002f;
    min = util::max(epsilon, min);
    max = util::max(epsilon, max);

    return min * powf(max / min, util::clamp(0.0f, 1.0f, t));
}

bool widgets::adsr_ui(const std::string &id, ImVec2 size, widgets::adsr_ui_struct *data)
{
    ImGui::BeginGroup();

    enum class DragTarget
    {
        NONE, ATTACK, DECAY, SUSTAIN, RELEASE
    };

    if (size.y == 0.0f)
    {
        size.y = ImGui::GetFrameHeight() * 1.5f;
    }

    uint32_t sep_color = ImGui::GetColorU32(ImGuiCol_ButtonActive);
    uint32_t graph_color = ImGui::GetColorU32(ImGuiCol_Button);

    Vec2 text_cursor = ImGui::GetCursorPos();
    ImGui::NewLine();

    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    Vec2 screen_origin = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton(id.c_str(), size);

    // draw background
    draw_list->AddRectFilled(screen_origin, screen_origin + size, ImGui::GetColorU32(ImGuiCol_PopupBg));
    draw_list->AddRect(screen_origin, screen_origin + size, ImGui::GetColorU32(ImGuiCol_Border));

    float left = size.x * (1.0f / 4.0f); // position of left sep bar
    float right = size.x * (3.0f / 4.0f); // position of right sep bar

    // draw attack curve
    //float atk_mul = data->attack / data->attack_max;
    float atk_mul = to_parametric_log(data->attack, 0.0f, data->attack_max);
    float atk_pos = atk_mul * left;
    //draw_list->AddTriangleFilled(screen_origin + Vec2(left, 0.0f), screen_origin + Vec2(atk_pos, size.y), screen_origin + Vec2(left, size.y), graph_color);
    draw_list->AddTriangleFilled(screen_origin + Vec2(atk_pos, 0.0f), screen_origin + Vec2(0.0f, size.y), screen_origin + Vec2(atk_pos, size.y), graph_color);

    float sustain_top = (1.0f - data->sustain) * size.y;

    // draw decay curve
    float dky_mul = to_parametric_log(data->decay, 0.0f, data->decay_max);
    float dky_scale = (right - 32.0f - left);
    float dky_pos = dky_mul * dky_scale + atk_pos;
    draw_list->AddQuadFilled(
        screen_origin + Vec2(dky_pos, size.y),
        screen_origin + Vec2(dky_pos, sustain_top),
        screen_origin + Vec2(atk_pos, 0.0f),
        screen_origin + Vec2(atk_pos, size.y),
        graph_color
    );

    // draw sustain
    draw_list->AddRectFilled(screen_origin + Vec2(dky_pos, sustain_top), screen_origin + Vec2(right, size.y), graph_color);

    // draw release curve
    float rls_mul = to_parametric_log(data->release, 0.0f, data->release_max);
    draw_list->AddTriangleFilled(screen_origin + Vec2(rls_mul * (size.x - right) + right, size.y), screen_origin + Vec2(right, sustain_top), screen_origin + Vec2(right, size.y), graph_color);

    // draw separator bar
    draw_list->AddLine(screen_origin + Vec2(right, 0.0f), screen_origin + Vec2(right, size.y), sep_color);

    // determine if this widget is active or not,
    // and which component to modify based off of mouse position in widget
    bool changed = false;
    static struct
    {
        DragTarget drag_target = DragTarget::NONE;
        float parametric_value;
    } active;

    DragTarget drag_target = DragTarget::NONE;

    if (!ImGui::IsAnyItemActive())
        active.drag_target = DragTarget::NONE;

    if (ImGui::IsItemActive() && active.drag_target != DragTarget::NONE)
    {
        // this widget is active here
        drag_target = active.drag_target;

        float dx = ImGui::GetIO().MouseDelta.x;
        float cursor_x = ImGui::GetMousePos().x - screen_origin.x;
        changed = dx != 0.0f;

        if (drag_target == DragTarget::ATTACK)
        {
            active.parametric_value += dx / left;
            data->attack = from_parametric_log(active.parametric_value, 0.0f, data->attack_max);
        }

        else if (drag_target == DragTarget::DECAY)
        {
            active.parametric_value += dx / dky_scale;
            data->decay = from_parametric_log(active.parametric_value, 0.0f, data->decay_max);
        }
            //data->decay = std::clamp(data->decay + dx / dky_scale * data->decay_max, 0.0f, data->decay_max);

        else if (drag_target == DragTarget::SUSTAIN)
        {
            float dy = ImGui::GetIO().MouseDelta.y;
            changed = dy != 0.0f;
            active.parametric_value -= dy / size.y;
            data->sustain = std::clamp(active.parametric_value, 0.0f, 1.0f);
        }

        else if (drag_target == DragTarget::RELEASE)
        {
            active.parametric_value += dx / (size.x - right);
            data->release = from_parametric_log(active.parametric_value, 0.0f, data->release_max);
        }
        
        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
    }
    else if (ImGui::IsItemHovered())
    {
        // this widget isn't active but is hovered
        Vec2 mouse_cursor = Vec2(ImGui::GetMousePos()) - screen_origin;
        
        float t;

        if (mouse_cursor.x < atk_pos + 10.0f)
        {
            drag_target = DragTarget::ATTACK;
            t = to_parametric_log(data->attack, 0.0f, data->attack_max);
        }

        else if (mouse_cursor.x < dky_pos + 20.0f)
        {
            drag_target = DragTarget::DECAY;
            t = to_parametric_log(data->decay, 0.0f, data->decay_max);
        }

        else if (mouse_cursor.x < right - 5.0f)
        {
            drag_target = DragTarget::SUSTAIN;
            t = data->sustain;
        }

        else
        {
            drag_target = DragTarget::RELEASE;
            t = to_parametric_log(data->release, 0.0f, data->release_max);
        }

        if (ImGui::IsItemActivated())
        {
            active.drag_target = drag_target;
            active.parametric_value = t;
        }
    }

    // hint text
    ImVec2 end_cursor = ImGui::GetCursorPos();
    ImGui::SetCursorPos(text_cursor);

    if (drag_target != DragTarget::NONE)
    {
        if (drag_target == DragTarget::ATTACK)
            ImGui::TextDisabled("Attack: %.0f ms", data->attack * 1000.0f);

        else if (drag_target == DragTarget::DECAY)
            ImGui::TextDisabled("Decay: %.0f ms", data->decay * 1000.0f);

        else if (drag_target == DragTarget::SUSTAIN)
            ImGui::TextDisabled("Sustain: %.0f%%", data->sustain * 100.0f);

        else if (drag_target == DragTarget::RELEASE)
            ImGui::TextDisabled("Release: %.0f ms", data->release * 1000.0f);
    }
    else
    {
        ImGui::Text("%s", id.c_str());
    }
    ImGui::SetCursorPos(end_cursor);
    ImGui::EndGroup();

    return changed;
}