#pragma once
#include <cmath>
#include "shortcuts.hpp"
#include "song.hpp"

namespace sbox
{
    // vector2 class fully compatible with ImGui's Vec2
    // this is so i can do vector math easily
    struct Vec2 {
        float x, y;
        
        constexpr Vec2() : x(0.0f), y(0.0f) {}
        constexpr Vec2(float _x, float _y) : x(_x), y(_y) {}
        Vec2(const ImVec2& src): x(src.x), y(src.y) {}

        Vec2 operator+(const Vec2& other) const {
            return Vec2(x + other.x, y + other.y);
        }

        Vec2 operator-(const Vec2& other) const {
            return Vec2(x - other.x, y - other.y);
        }

        Vec2 operator*(const Vec2& other) const {
            return Vec2(x * other.x, y * other.y);
        }

        Vec2 operator/(const Vec2& other) const {
            return Vec2(x / other.x, y / other.y);
        }

        bool operator==(const Vec2& other) const {
            return x == other.x && y == other.y;
        }

        bool operator!=(const Vec2& other) const {
            return x != other.x || y != other.y;
        }

        float magn_sq() const {
            return x * x + y * y; 
        }

        float magn() const {
            return sqrtf(x * x + y * y);
        }

        template <typename T>
        Vec2 operator*(const T& scalar) const {
            return Vec2(x * scalar, y * scalar);
        }

        template <typename T>
        Vec2 operator/(const T& scalar) const {
            return Vec2(x / scalar, y / scalar);
        }

        operator ImVec2() const { return ImVec2(x, y); }
    };

    class SongEditor
    {
    private:
        void render_song_settings();
        void render_channel_settings();
        void render_track_editor();
        void render_pattern_editor();

        void play_note(unsigned int channel, unsigned int key, float velocity, float duration);
    
    public:
        Song &song;
        ShortcutContext &shortcuts;

        uint selected_channel;
        uint selected_bar;
        float quantization;
        bool note_preview;
        bool show_all_channels;

        SongEditor(Song& song, ShortcutContext &shortcuts);

        void draw();
    };
}