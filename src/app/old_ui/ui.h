/**
* The UI directory holds all source code related to
* ImGui rendering. The state of the UI is held
* in the SongEditor class (in editor/editor.h)
**/
#pragma once

#include <cstdint>
#include <iostream>
#include <imgui.h>

#include "../audio.h"
#include "../song.h"
#include "../editor/editor.h"

#define IM_RGB32(R, G, B) IM_COL32(R, G, B, 255)

namespace ui
{
    // vector2 class fully compatible with ImGui's Vec2
    // this is so i can do vector math easily
    struct Vec2 {
        float x, y;
        
        constexpr Vec2() : x(0.0f), y(0.0f) {}
        constexpr Vec2(float _x, float _y) : x(_x), y(_y) {}
        Vec2(const ImVec2& src): x(src.x), y(src.y) {}

        inline Vec2 operator+(const Vec2& other) const {
            return Vec2(x + other.x, y + other.y);
        }

        inline Vec2 operator-(const Vec2& other) const {
            return Vec2(x - other.x, y - other.y);
        }

        inline Vec2 operator*(const Vec2& other) const {
            return Vec2(x * other.x, y * other.y);
        }

        inline Vec2 operator/(const Vec2& other) const {
            return Vec2(x / other.x, y / other.y);
        }

        inline bool operator==(const Vec2& other) const {
            return x == other.x && y == other.y;
        }

        inline bool operator!=(const Vec2& other) const {
            return x != other.x || y != other.y;
        }

        inline float magn_sq() const {
            return x * x + y * y; 
        }

        inline float magn() const {
            return sqrtf(x * x + y * y);
        }

        template <typename T>
        inline Vec2 operator*(const T& scalar) const {
            return Vec2(x * scalar, y * scalar);
        }

        template <typename T>
        inline Vec2 operator/(const T& scalar) const {
            return Vec2(x / scalar, y / scalar);
        }

        inline operator ImVec2() const { return ImVec2(x, y); }
    };

    extern bool show_demo_window;

    inline ImU32 vec4_color(const ImVec4 &vec4) {
        return IM_COL32(int(vec4.x * 255), int(vec4.y * 255), int(vec4.z * 255), int(vec4.w * 255));
    }

    void ui_init(SongEditor& editor);
    void compute_imgui(SongEditor& editor);

    enum class FileBrowserMode
    {
        Save, Open, Directory
    };

    std::string file_browser(FileBrowserMode mode, const std::string& filter_list, const std::string& default_path);

    void render_song_settings(SongEditor& editor);
    void render_channel_settings(SongEditor& editor);
    void render_track_editor(SongEditor& editor);
    void render_pattern_editor(SongEditor& editor);
    void render_fx_mixer(SongEditor& editor);

    void render_plugin_list(SongEditor& editor);
    void render_directories_window(SongEditor& editor);
    void render_tunings_window(SongEditor& editor);
    void render_themes_window(SongEditor& editor);
    void render_export_window(SongEditor& editor);

    const char* module_selection_popup(SongEditor& editor, bool instruments);

    // these functions make it so the following items
    // are slightly transparent if disabled
    void push_btn_disabled(ImGuiStyle& style, bool is_disabled);
    void pop_btn_disabled();

    void show_status(const char* fmt, ...);
    void show_status(const std::string& status_text);
    void prompt_unsaved_work(std::function<void()> callback);

    // enum used for the type of the value in ChangeActionType
    enum class ValueType : uint8_t
    {
        Null = (uint8_t) 0,
        Float,
        Int
    };

    // custom input behavior for undo/redo
    // TODO: does not work for properly for char*
    template <typename T>
    bool change_detection(SongEditor& editor, T value, T* previous_value_out, ImGuiID id = ImGui::GetItemID(), bool always_check = false)
    {
        // if previous value for this id is not found
        // write initial value
        void* val_alloc;

        if (editor.ui_values.find(id) == editor.ui_values.end())
        {
            val_alloc = malloc(sizeof(T));
            memcpy(val_alloc, &value, sizeof(T));
            editor.ui_values[id] = val_alloc;
        }
        else
            val_alloc = editor.ui_values[id];

        if (always_check || ImGui::IsItemDeactivatedAfterEdit())
        {
            T* previous_value = static_cast<T*>(val_alloc);

            if (*previous_value != value) {
                std::cout << *previous_value << " -> " << value << "\n";
                *previous_value_out = *previous_value;

                // write new previous value
                memcpy(previous_value, &value, sizeof(T));

                return true;
            }
            else {
                if (!always_check) std::cout << "no change\n";
            }
        }

        return false;
    }

    enum class EffectsInterfaceAction
    {
        Nothing, Edit, Delete, Add, Swapped, SwapInstrument
    };

    struct EffectsInterfaceResult
    {
        const char* module_id;
        const char* plugin_id; // if module_id == "plugin", use this
        int target_index;
        int swap_start;
        int swap_end;    
    };

    EffectsInterfaceAction effect_rack_ui(
        SongEditor* editor,
        audiomod::EffectsRack* rack,
        EffectsInterfaceResult* result,
        bool has_instrument = false
    );
}