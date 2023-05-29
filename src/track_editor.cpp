#include <imgui.h>
#include "song.h"
#include "ui.h"

template <typename T>
static inline T max(T a, T b) {
    return a > b ? a : b;
}

template <typename T>
static inline T min(T a, T b) {
    return a < b ? a : b;
}

void render_track_editor(ImGuiIO &io, Song &song)
{
    ImGuiStyle& style = ImGui::GetStyle();

    if (ImGui::Begin("Track Editor")) {
        // width allocated for the mute and solo controls
        float btn_text_size = ImGui::CalcTextSize("M").x;
        float controls_width = 5.0f + 2.0f * (btn_text_size + style.FramePadding.x * 2.0f) + style.ItemSpacing.x;
        // cell size including margin
        const Vec2 CELL_SIZE = Vec2(int(ImGui::GetFrameHeightWithSpacing() * 1.1f), int(ImGui::GetFrameHeightWithSpacing() * 1.1f));
        // empty space inbetween cells
        static const int CELL_MARGIN = 1;
        // space dedicated to channel properties
        // the max characters allowed for a channel name is 16 characters
        // so we test the width of 16 M's (the widest character in most fonts)
        const float CHANNEL_COLUMN_WIDTH = ImGui::CalcTextSize("MMMMMMMMMMMMMMMM").x + controls_width + 5.0f;

        static int num_channels = song.channels.size();
        static int num_bars = song.length();

        ImGui::BeginChild("###track_editor_area", Vec2(-1, -1), false, ImGuiWindowFlags_HorizontalScrollbar);
        
        Vec2 canvas_size = ImGui::GetContentRegionAvail();
        Vec2 canvas_p0 = ImGui::GetCursorScreenPos();
        Vec2 canvas_p1 = canvas_p0 + canvas_size;
        Vec2 viewport_scroll = (Vec2)ImGui::GetWindowPos() - canvas_p0;
        Vec2 mouse_pos = Vec2(io.MousePos) - canvas_p0 - Vec2(CHANNEL_COLUMN_WIDTH, 0.0f);
        Vec2 content_size = Vec2(num_bars, num_channels) * CELL_SIZE + Vec2(CHANNEL_COLUMN_WIDTH, 0.0f);

        int mouse_row = -1;
        int mouse_col = -1;

        ImGui::SetCursorPos(Vec2(CHANNEL_COLUMN_WIDTH, 0.0f));
        ImGui::InvisibleButton("track_editor_mouse_target", content_size - Vec2(CHANNEL_COLUMN_WIDTH, 0.0f), ImGuiButtonFlags_MouseButtonLeft);
        if (ImGui::IsItemHovered()) {
            mouse_row = (int)mouse_pos.y / CELL_SIZE.y;
            mouse_col = (int)mouse_pos.x / CELL_SIZE.x;

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                song.selected_bar = mouse_col;
                song.selected_channel = mouse_row;
            }
        }
        
        // use canvas for rendering
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        // visible bounds of viewport
        int bar_start = (int)viewport_scroll.x / CELL_SIZE.x;
        int bar_end = bar_start + (int)canvas_size.x / CELL_SIZE.x + 2;
        int col_start = (int)viewport_scroll.y / CELL_SIZE.y;
        int col_end = col_start + (int)canvas_size.y / CELL_SIZE.y + 2;
        
        char str_buf[8];

        // draw patterns
        Vec2 view_origin = (Vec2)ImGui::GetWindowPos() + Vec2(CHANNEL_COLUMN_WIDTH, 0.0f);
        ImGui::PushClipRect(
            view_origin,
            view_origin + content_size - Vec2(CHANNEL_COLUMN_WIDTH, 0.0f),
            true
        );

        for (int ch = col_start; ch < min(col_end, num_channels); ch++) {
            for (int bar = bar_start; bar < min(bar_end, num_bars); bar++) {
                Vec2 rect_pos = Vec2(canvas_p0.x + bar * CELL_SIZE.x + CELL_MARGIN + CHANNEL_COLUMN_WIDTH, canvas_p0.y + CELL_SIZE.y * ch + CELL_MARGIN);
                int pattern_num = song.channels[ch]->sequence[bar];
                bool is_selected = song.selected_bar == bar && song.selected_channel == ch;

                // draw cell background
                if (pattern_num > 0 || is_selected)
                    draw_list->AddRectFilled(
                        rect_pos,
                        Vec2(rect_pos.x + CELL_SIZE.x - CELL_MARGIN * 2, rect_pos.y + CELL_SIZE.y - CELL_MARGIN * 2),
                        is_selected ? Colors::channel[ch % Colors::channel_num][1] : vec4_color(style.Colors[ImGuiCol_FrameBg])
                    );
                
                snprintf(str_buf, 8, "%i", pattern_num); // convert pattern_num to string (too lazy to figure out how to do it the C++ way)
                
                // draw pattern number
                draw_list->AddText(
                    rect_pos + (CELL_SIZE - Vec2(CELL_MARGIN, CELL_MARGIN) * 2.0f - ImGui::CalcTextSize(str_buf)) / 2.0f,
                    is_selected ? IM_COL32_BLACK : Colors::channel[ch % Colors::channel_num][pattern_num > 0],
                    str_buf
                );

                // draw mouse hover
                if (ch == mouse_row && bar == mouse_col) {
                    Vec2 rect_pos = Vec2(canvas_p0.x + bar * CELL_SIZE.x + CHANNEL_COLUMN_WIDTH, canvas_p0.y + CELL_SIZE.y * ch);
                    draw_list->AddRect(rect_pos, rect_pos + CELL_SIZE, vec4_color(style.Colors[ImGuiCol_Text]), 0.0f, 0, 1.0f);
                }
            }
        }

        // draw playhead
        double song_pos = song.is_playing ? (song.position / song.beats_per_bar) : (song.bar_position);
        Vec2 playhead_pos = canvas_p0 + Vec2(song_pos * CELL_SIZE.x + CHANNEL_COLUMN_WIDTH, 0);
        draw_list->AddRectFilled(playhead_pos, playhead_pos + Vec2(1.0f, canvas_size.y), vec4_color(style.Colors[ImGuiCol_Text]));

        ImGui::PopClipRect();

        // create colors for disabled buttons
        ImVec4 disabled_btn_colors[3];
        disabled_btn_colors[0] = style.Colors[ImGuiCol_Button];
        disabled_btn_colors[1] = style.Colors[ImGuiCol_ButtonHovered];
        disabled_btn_colors[2] = style.Colors[ImGuiCol_ButtonActive];

        for (int i = 0; i < 3; i++) {
            disabled_btn_colors[i].w *= 0.5f;
        }

        // draw channel info
        for (int ch = col_start; ch < min(col_end, num_channels); ch++) {
            ImGui::PushID(ch);

            Vec2 row_start = Vec2(
                viewport_scroll.x,
                CELL_SIZE.y * ch + CELL_MARGIN + (CELL_SIZE.y - ImGui::GetTextLineHeightWithSpacing()) / 2.0f
            );
            
            ImGui::SetCursorPos(row_start);
            ImGui::Text("%s", song.channels[ch]->name);
            
            ImGui::SetCursorPos(row_start + Vec2(
                CHANNEL_COLUMN_WIDTH - controls_width,
                0.0f)
            );

            Channel* ch_dat = song.channels[ch];
            bool enable_mute = ch_dat->vol_mod.mute;
            bool enable_solo = ch_dat->solo;

            if (!enable_mute) {
                ImGui::PushStyleColor(ImGuiCol_Button, disabled_btn_colors[0]);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, disabled_btn_colors[1]);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, disabled_btn_colors[2]);
            }

            // mute button
            if (ImGui::SmallButton("M")) {
                ch_dat->vol_mod.mute = !enable_mute;
            }

            if (!enable_mute) ImGui::PopStyleColor(3);

            ImGui::SameLine();

            if (!enable_solo) {
                ImGui::PushStyleColor(ImGuiCol_Button, disabled_btn_colors[0]);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, disabled_btn_colors[1]);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, disabled_btn_colors[2]);
            }

            // solo button
            if (ImGui::SmallButton("S")) {
                ch_dat->solo = !ch_dat->solo;
            }

            if (!enable_solo) ImGui::PopStyleColor(3);
            
            ImGui::PopID();
        }

        // set scrollable area
        //ImGui::SetCursorPos(content_size);

        ImGui::EndChild();
    } ImGui::End();
}