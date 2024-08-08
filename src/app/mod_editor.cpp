#include <cfloat>
#include <imgui.h>
#include "audio_engine/audio_engine.hpp"
#include "log.hpp"
#include "mod_editor.hpp"
#include "shortcuts.hpp"

using namespace sbox;

ModuleEditor::ModuleEditor(Song &song, ShortcutContext &shortcuts) :
    song(song),
    shortcuts(shortcuts)
{}

void ModuleEditor::draw()
{
    float mod_ui_height = ImGui::GetFontSize() * 17.0f;
    modules::ModuleID hovered_module_ui = 0;

    //ImGui::SetNextWindowSizeConstraints(ImVec2(0.0fmod_ui_width, 0.0f), ImVec2(mod_ui_width, FLT_MAX));

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

            ImGuiChildFlags child_flags = ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_Border;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetColorU32(ImGuiCol_PopupBg, 0.4f));
            ImGui::BeginChild("module ui", ImVec2(0.0f, mod_ui_height), child_flags, ImGuiWindowFlags_MenuBar);
            ImGui::PopStyleColor();

            if (ImGui::BeginMenuBar())
            {
                ImVec2 start_cursor = ImGui::GetCursorPos();

                ImGui::Text("%s", mod->name().c_str());
                ImGui::Separator();
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

                //ImGui::SetCursorPos(start_cursor);
                ImVec2 drag_area_size = ImGui::GetContentRegionAvail();

                if (drag_area_size.x > 0.0f && drag_area_size.y > 0.0f)
                {
                    ImGui::Button("##DragArea", drag_area_size);

                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                        hovered_module_ui = mod->id();
                    }

                    if (ImGui::IsItemActivated())
                    {
                        logger::log_debug("begin module drag");
                    }
                }

                ImGui::EndMenuBar();
            }

            mod_data->ui();
            ImGui::EndChild();
            ImGui::PopID();
        }
    } ImGui::End();
}