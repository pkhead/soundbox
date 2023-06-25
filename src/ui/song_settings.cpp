#include "ui.h"
#include "imgui_stdlib.h"

void render_song_settings(SongEditor &editor)
{
    Song& song = *editor.song;
    UserActionList& user_actions = editor.ui_actions;

    if (ImGui::Begin("Song Settings")) {
        // song name input
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Name");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##song_name", song.name, song.name_capcity);

        // play/prev/next
        if (ImGui::Button(song.is_playing ? "Pause##play_pause" : "Play##play_pause", ImVec2(-1.0f, 0.0f)))
            user_actions.fire("song_play_pause");
                
        if (ImGui::Button("Prev", ImVec2(ImGui::GetWindowSize().x / -2.0f, 0.0f))) user_actions.fire("song_prev_bar");
        ImGui::SameLine();
        if (ImGui::Button("Next", ImVec2(-1.0f, 0.0f))) user_actions.fire("song_next_bar");
        
        // tempo
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Tempo");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::DragFloat("###song_tempo", &song.tempo, 1.0f, 0.0f, 5000.0f, "%.3f");
        if (song.tempo < 0) song.tempo = 0;

        { // change detection
            float prev;
            if (change_detection(editor, song.tempo, &prev)) {
                editor.push_change(new change::ChangeSongTempo(ImGui::GetItemID(), prev, song.tempo));
            }
        }
        
        // TODO: controller/mod channels
        if (ImGui::BeginPopupContextItem()) {
            ImGui::Selectable("Add to selected modulator", false);
            ImGui::EndPopup();
        }

        // max patterns per channel
        int max_patterns = song.max_patterns();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Max Patterns");

        // patterns help
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 20.0f);
            
            ImGui::TextWrapped(
                "Each channel has an individual list of patterns, and this input "
                "controls the available amount of patterns per channel. "
                "You can use this input to add more patterns, but you can also "
                "place a new note on a null pattern (pattern 0) or select Edit > New Pattern"
            );
            
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        // max patterns input
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputInt("###song_patterns", &max_patterns);
        if (max_patterns >= 1)
        {
            int old_max_patterns = song.max_patterns();
            if (max_patterns != old_max_patterns)
            {
                editor.push_change(new change::ChangeSongMaxPatterns(old_max_patterns, max_patterns, &song));
                song.set_max_patterns(max_patterns);
            }
        }

        // project notes
        ImGui::Text("Project Notes");
        ImGui::InputTextMultiline_str("###project_notes", &song.project_notes, ImVec2(-1.0f, ImGui::GetTextLineHeight() * 16.0f));
    } ImGui::End();
}