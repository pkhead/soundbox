#include "ui.h"
using namespace ui;

void ui::render_themes_window(SongEditor &editor)
{
    if (editor.show_themes_window) {
        if (ImGui::Begin("Themes", &editor.show_themes_window, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (ImGui::BeginMenuBar())
            {
                if (ImGui::BeginMenu("File"))
                {
                    ImGui::Selectable("Show Theme Editor...");
                    ImGui::Selectable("Export Theme to File...");
                    ImGui::EndMenu();
                }

                ImGui::EndMenuBar();
            }

            for (const std::string& name : editor.theme.get_themes_list())
            {
                if (ImGui::Selectable(name.c_str(), name == editor.theme.name()))
                {
                    editor.theme.load(name);
                    editor.theme.set_imgui_colors();
                }
            }
        } ImGui::End();
    }
}