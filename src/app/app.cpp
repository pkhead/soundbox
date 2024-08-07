#include <memory>
#include "app.hpp"
#include "imgui.h"
#include "../modules/internal/host.hpp"

using namespace sbox;

Application::Application()
{
    running = true;

    _audio_engine.register_host(std::make_unique<hosts::internal::InternalModuleHost>());
    _song = std::make_unique<Song>(4, 4, 4, _audio_engine);
}

Application::~Application()
{}

void Application::request_close()
{
    running = false;
}

void Application::update(float dt)
{
    ImGui::ShowDemoWindow();
}