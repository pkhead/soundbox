#include "ui.h"

void render_directories_window(SongEditor &editor)
{
    ImGuiStyle& style = ImGui::GetStyle();
    plugins::PluginManager& plugins = editor.plugin_manager;

    static char test[64];

    if (editor.show_dir_window)
    {
        if (ImGui::Begin("Directories", &editor.show_dir_window, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("LADSPA/LV1 plugins");
            ImGui::SameLine();
            if (ImGui::Button("Add"))
                ImGui::OpenPopup("adddirectory");

            if (ImGui::BeginListBox(
                "##ladspa_plugins",
                ImVec2(0.0f, ImGui::GetFrameHeight() * plugins.ladspa_paths.size())
            ))
            {
                for (const std::string& path : plugins.ladspa_paths)
                {
                    ImGui::Selectable(path.c_str(), false);
                }

                ImGui::EndListBox();
            }

            if (ImGui::BeginPopup("adddirectory"))
            {
                ImGui::Text("Add Path");
                ImGui::InputText("##path", test, 64);
                ImGui::SameLine();
                ImGui::Button("Choose...");
                ImGui::EndPopup();
            }
        } ImGui::End();

    }
}