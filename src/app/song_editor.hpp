#pragma once
#include <cmath>
#include <vec2.hpp>
#include "shortcuts.hpp"
#include "song.hpp"

namespace sbox
{
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