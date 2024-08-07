#include "ui.h"
using namespace ui;

void ui::render_plugin_list(SongEditor &editor)
{
    static int selected_plugin = 0;

    if (editor.show_plugin_list && ImGui::Begin(
        "Plugin List",
        &editor.show_plugin_list,
        ImGuiWindowFlags_NoDocking
    ))
    {
        if (ImGui::Button("Rescan"))
            editor.plugin_manager.scan_plugins();
        
        auto& plugin_list = editor.plugin_manager.get_plugin_data();

        selected_plugin = -1;

        if (ImGui::BeginChild("list", ImVec2(ImGui::GetContentRegionAvail().x * 0.4f, -1.0f)))
        {
            int i = 0;
            for (auto& plugin_data : plugin_list)
            {
                ImGui::PushID(i);
                ImGui::Selectable(plugin_data.name.c_str(), i == selected_plugin);
                if (ImGui::IsItemHovered())
                    selected_plugin = i;

                ImGui::PopID();
                i++;
            }
        }

        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginGroup();

        if (selected_plugin >= 0)
        {
            auto& plugin_data = plugin_list[selected_plugin];

            ImGui::Text("File: %s", plugin_data.file_path.c_str());
            ImGui::Separator();
            ImGui::Text("Author: %s", plugin_data.author.c_str());
            ImGui::Separator();
            ImGui::Text("Copyright: %s", plugin_data.copyright.c_str());
            ImGui::Separator();

            const char* plugin_type;

            switch (plugin_data.type)
            {
                case plugins::PluginType::Ladspa:
                    plugin_type = "LADSPA";
                    break;

                case plugins::PluginType::Lv2:
                    plugin_type = "LV2";
                    break;

                case plugins::PluginType::Clap:
                    plugin_type = "CLAP";
                    break;

                case plugins::PluginType::Vst:
                    plugin_type = "VST2";
                    break;
            }

            ImGui::Text("Is Instrument?: %s", plugin_data.is_instrument ? "Yes": "No");
            ImGui::Separator();
            ImGui::Text("Type: %s", plugin_type);
        }

        ImGui::EndGroup();
        ImGui::End();
    }
}