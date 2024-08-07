#pragma once
#include <memory>
#include "../audio_engine/audio_engine.hpp"
#include "song.hpp"

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

    public:
        Application();
        ~Application();

        bool running;

        void update(float dt);
        void request_close();
    }; // class Application
}