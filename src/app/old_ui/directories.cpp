#include <filesystem>
#include <algorithm>
#include "ui.h"
using namespace ui;

void ui::render_directories_window(SongEditor &editor)
{
    ImGuiStyle& style = ImGui::GetStyle();
    plugins::PluginManager& plugins = editor.plugin_manager;

    static char dir_prompt[128];

    if (editor.show_dir_window)
    {
        if (ImGui::Begin(
            "Directories",
            &editor.show_dir_window,
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoDocking
        )) {
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

            std::vector<std::filesystem::path> ladspa_plugins = plugins.get_paths(plugins::PluginType::Ladspa);
            int listbox_size = ladspa_plugins.size();
            if (listbox_size == 0) listbox_size = 1;

            if (ImGui::BeginListBox(
                "##ladspa_plugins",
                ImVec2(-1.0f, ImGui::GetFrameHeight() * listbox_size)
            ))
            {
                std::string path_to_delete;
                path_to_delete.clear();

                int i = 0;
                for (const std::filesystem::path& path : ladspa_plugins)
                {
                    ImGui::Selectable(path.u8string().c_str(), false);
                    
                    if (ImGui::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonLeft))
                    {
                        // tell user they can't remove standard paths
                        // this is only because application adds
                        // standard paths on startup
                        if (!plugins.is_user_path(plugins::PluginType::Ladspa, path)) {
                            ImGui::TextDisabled("Remove");
                        } else {
                            if (ImGui::Selectable("Remove"))
                                path_to_delete = path;
                        }

                        ImGui::EndPopup();
                    }

                    i++;
                }

                ImGui::EndListBox();

                if (!path_to_delete.empty())
                    plugins.remove_path(plugins::PluginType::Ladspa, path_to_delete);
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
                    std::string path_str = std::filesystem::current_path().root_path().string();
                    const char* default_path = path_str.c_str();
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