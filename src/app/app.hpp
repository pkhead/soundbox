#pragma once
#include <memory>
#include <unordered_map>
#include "../audio_engine/audio_engine.hpp"
#include "imgui.h"
#include "mod_editor.hpp"
#include "theme.hpp"
#include "song.hpp"
#include "song_editor.hpp"

namespace sbox
{
    /**
    * Main app loop.
    **/
    class Application
    {
    private:
        modules::AudioEngine _audio_engine;
        std::unique_ptr<Song> _song;
        std::unique_ptr<SongEditor> _song_editor;

        ModuleList module_list;
        std::unique_ptr<ModuleEditor> _inst_editor;
        std::unique_ptr<ModuleEditor> _fx_editor;

        bool _show_imgui_demo_window;

        void handle_shortcuts();

    public:
        static Application *instance;
        
        Application();
        ~Application();

        bool running;
        Theme theme;
        ShortcutContext shortcut_ctx;

        void update(float dt);
        void request_close();

        void draw_interface();
    }; // class Application
}