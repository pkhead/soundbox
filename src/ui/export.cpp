#include "ui.h"
using namespace ui;

void ui::render_export_window(SongEditor &editor)
{
    auto& export_config = editor.export_config;

    if (export_config.active) {
        if (ImGui::Begin("Export", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Export path: %s", export_config.file_name);
            ImGui::SameLine();
            if (ImGui::SmallButton("Choose...")) {
                editor.ui_actions.fire("export");
            };

            static int rate_sel = 2;

            static const char* rate_options[] = {
                "96 kHz",
                "88.2 kHz",
                "48 kHz (recommended)",
                "44.1 kHz",
                "22.05 kHz"
            };

            static int rate_values[] = {
                96000,
                88200,
                48000,
                44100,
                22050
            };

            static const char* rate_option = "48 kHz";
            
            // sample rate selection
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Sample Rate");
            ImGui::SameLine();
            if (ImGui::BeginCombo("###export_sample_rate", rate_options[rate_sel])) {
                for (int i = 0; i < IM_ARRAYSIZE(rate_options); i++) {
                    if (ImGui::Selectable(rate_options[i], rate_sel == i)) {
                        rate_sel = i;
                        export_config.sample_rate = rate_values[i];
                    }

                    if (rate_sel == i) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                
                ImGui::EndCombo();
            }

            if (ImGui::Button("Export")) {
                editor.begin_export();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                editor.stop_export();
            }

            ImVec2 bar_size = ImVec2(ImGui::GetTextLineHeight() * 20.0f, 0.0f);
            ImGui::SameLine();

            const std::unique_ptr<SongExport>& song_export = editor.current_export();

            if (song_export) {
                ImGui::ProgressBar(song_export->get_progress(), bar_size);

                if (song_export->finished()) {
                    editor.export_config.active = false;
                    editor.stop_export();

                    ui::show_status("Successfully exported to %s", export_config.file_name);
                }
            } else {
                ImGui::ProgressBar(0.0f, bar_size, "");
            }

            ImGui::End();
        }
    }
}