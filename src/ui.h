#pragma once

#include <functional>
#include <math.h>
#include "song.h"
#include <imgui.h>

#define IM_RGB32(R, G, B) IM_COL32(R, G, B, 255)

namespace Colors {
    constexpr size_t channel_num = 4;
    extern ImU32 channel[channel_num][2];
}

constexpr uint8_t USERMOD_SHIFT = 1;
constexpr uint8_t USERMOD_CTRL = 2;
constexpr uint8_t USERMOD_ALT = 4;

struct UserAction {
    std::string name;
    uint8_t modifiers;
    ImGuiKey key;
    std::function<void()> callback;
    std::string combo;
    bool do_repeat;

    UserAction(const std::string& name, uint8_t mod, ImGuiKey key, bool repeat = false);
    void set_keybind(uint8_t mod, ImGuiKey key);
};

struct UserActionList {
    std::vector<UserAction> actions;

    UserActionList();
    void add_action(const std::string& action_name, uint8_t mod, ImGuiKey key, bool repeat = false);
    void fire(const std::string& action_name) const;
    void set_keybind(const std::string& action_name, uint8_t mod, ImGuiKey key);
    void set_callback(const std::string& action_name, std::function<void()> callback);
    const char* combo_str(const std::string& action_name) const;

    /*
    UserAction song_save;
    UserAction song_save_as;
    UserAction song_open;
    
    UserAction song_play_pause;
    UserAction song_prev_bar;
    UserAction song_next_bar;
    UserAction quit;
    */
};

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

void ui_init(Song& song, UserActionList& actions);
void compute_imgui(ImGuiIO& io, Song& song, UserActionList& actions);

void render_track_editor(ImGuiIO& io, Song& song);
void render_pattern_editor(ImGuiIO& io, Song& song);