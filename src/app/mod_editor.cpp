#include <cfloat>
#include <imgui.h>
#include "mod_editor.hpp"
#include "shortcuts.hpp"

using namespace sbox;

ModuleEditor::ModuleEditor(Song &song, ShortcutContext &shortcuts) :
    song(song),
    shortcuts(shortcuts)
{}

void ModuleEditor::draw()
{
    float mod_ui_width = ImGui::GetFontSize() * 18.0f;
    ImGui::SetNextWindowSizeConstraints(ImVec2(mod_ui_width, 0.0f), ImVec2(mod_ui_width, FLT_MAX));

    if (ImGui::Begin("Module Editor", nullptr))
    {
        assert(selected_channel_type == CHANNEL_TYPE_INSTRUMENT);
        InstrumentChannel &channel = song.get_channel(selected_channel);

        for (unsigned int i = 0; i < channel.rack.size(); i++)
        {
            modx::ModuleRc &mod = channel.rack.at(0);
            modx::ModuleBase *mod_data = modx::ModuleHost::get_module(mod->id());
            assert(mod_data != nullptr);

            ImGui::PushID(i);
            ImGuiChildFlags child_flags = ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_Border;
            ImGui::BeginChild("module ui", ImVec2(mod_ui_width, 0.0f), child_flags, ImGuiWindowFlags_MenuBar);
            if (ImGui::BeginMenuBar())
            {
                if (ImGui::BeginMenu("Presets"))
                {
                    ImGui::MenuItem("Save Preset...");
                    if (ImGui::BeginMenu("Load Preset"))
                    {
                        for (int i = 0; i < 30; i++)
                        {
                            ImGui::MenuItem("Preset");
                        }
                        ImGui::EndMenu();
                    }
                    ImGui::EndMenu();
                }

                ImGui::EndMenuBar();
            }

            mod_data->ui();
            ImGui::EndChild();
            ImGui::PopID();
        }
    } ImGui::End();
}