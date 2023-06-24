#include "ui.h"

void render_tunings_window(SongEditor &editor)
{
    Song& song = *editor.song;
    UserActionList& user_actions = editor.ui_actions;
    
    if (editor.show_tuning_window) {
        if (ImGui::Begin("Tunings", &editor.show_tuning_window, ImGuiWindowFlags_NoDocking))
        {
            if (ImGui::Button("Load"))
                user_actions.fire("load_tuning");

            if (ImGui::BeginChild("list", ImVec2(ImGui::GetContentRegionAvail().x * 0.4f, -1.0f)))
            {
                int i = 0;
                int index_to_delete = -1;

                for (Tuning* tuning : song.tunings)
                {
                    ImGui::PushID(i);

                    if (ImGui::Selectable(tuning->name.c_str(), i == song.selected_tuning))
                        song.selected_tuning = i;

                    // right-click to remove (except 12edo, that can't be deleted)
                    if (i > 0 && ImGui::BeginPopupContextItem())
                    {
                        if (ImGui::Selectable("Remove"))
                            index_to_delete = i;
                        ImGui::EndPopup();
                    }

                    if (song.selected_tuning == i) ImGui::SetItemDefaultFocus();

                    ImGui::PopID();
                    i++;
                }

                // if a deletion was requested
                if (index_to_delete >= 0) {
                    if (song.selected_tuning == index_to_delete) {
                        song.selected_tuning--;
                    }

                    song.tunings.erase(song.tunings.begin() + index_to_delete);
                }
            }

            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginGroup();

            Tuning* selected_tuning = song.tunings[song.selected_tuning];

            ImGui::Text("Description");
            ImGui::Separator();

            if (selected_tuning->desc.empty())
                ImGui::TextDisabled("(no description)");
            else
                ImGui::TextWrapped("%s", selected_tuning->desc.c_str());

            // file extension info (exists to tell the user if they can apply kbm to the tuning)
            if (selected_tuning->is_12edo)
                ImGui::NewLine();
            else if (selected_tuning->scl_import == nullptr)
                ImGui::Text("TUN file");
            else
            {
                ImGui::Text("SCL file");

                // kbm import help
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
                    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 15.0f);
                    
                    ImGui::Text("Loading a .kbm file while an .scl tuning is selected will apply the keyboard mapping to the scale.");

                    ImGui::PopTextWrapPos();
                    ImGui::EndTooltip();
                }
            }

            ImGui::EndGroup();
        } ImGui::End();
    }
}