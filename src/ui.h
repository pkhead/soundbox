#pragma once

#include <functional>
#include "song.h"
#include "../imgui/imgui.h"

struct UserActionList {
    std::function<void()> song_play_pause;
    std::function<void()> song_prev_bar;
    std::function<void()> song_next_bar;
    std::function<void()> quit;
};

extern bool show_demo_window;
void compute_imgui(ImGuiIO& io, Song& song, UserActionList& actions);
