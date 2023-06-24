#include <filesystem>
#include "ui.h"

void render_directories_window(SongEditor &editor)
{
    ImGuiStyle& style = ImGui::GetStyle();
    plugins::PluginManager& plugins = editor.plugin_manager;

    static char dir_prompt[128];

    if (editor.show_dir_window)
    {
        if (ImGui::Begin("Directories", &editor.show_dir_window, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("LADSPA/LV1 plugins");
            ImGui::SameLine();
            if (ImGui::Button("Add")) {
                dir_prompt[0] = 0;
                ImGui::OpenPopup("adddirectory");
            }

            ImGui::SameLine();
            if (ImGui::Button("Rescan"))
                plugins.scan_plugins();

            int listbox_size = plugins.ladspa_paths.size();
            if (listbox_size == 0) listbox_size = 1;

            if (ImGui::BeginListBox(
                "##ladspa_plugins",
                ImVec2(-1.0f, ImGui::GetFrameHeight() * listbox_size)
            ))
            {
                int index_to_delete = -1;

                int i = 0;
                for (const std::string& path : plugins.ladspa_paths)
                {
                    ImGui::Selectable(path.c_str(), false);
                    
                    if (ImGui::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonLeft))
                    {
                        if (ImGui::Selectable("Remove"))
                            index_to_delete = i;

                        ImGui::EndPopup();
                    }

                    i++;
                }

                ImGui::EndListBox();

                if (index_to_delete >= 0)
                    plugins.ladspa_paths.erase(plugins.ladspa_paths.begin() + index_to_delete);
            }

            if (ImGui::BeginPopup("adddirectory"))
            {
                if (ImGui::IsWindowAppearing())
                    ImGui::SetKeyboardFocusHere();
                
                if (ImGui::InputText("##path", dir_prompt, 128, ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    plugins.add_path(plugins::PluginType::Ladspa, dir_prompt);
                    ImGui::CloseCurrentPopup();
                }

                ImGui::SameLine();
                if (ImGui::Button("Browse..."))
                {
                    const char* default_path = std::filesystem::current_path().root_path().c_str();
                    std::string result = file_browser(FileBrowserMode::Directory, "", default_path);
                    if (!result.empty()) {
                        plugins.add_path(plugins::PluginType::Ladspa, result);
                        ImGui::CloseCurrentPopup();
                    }
                }

                ImGui::EndPopup();
            }
        } ImGui::End();

    }
}