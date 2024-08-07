#include <imgui.h>
#include "../editor/change_history.h"
#include "../song.h"
#include "ui.h"
using namespace ui;

void ui::render_track_editor(SongEditor& editor)
{
    Song& song = *editor.song;
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();

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

    static int last_cursor_x = editor.selected_bar;
    static int last_cursor_y = editor.selected_channel;
    static int last_width = song.length();
    static int last_height = song.channels.size();

    int num_channels = song.channels.size();
    int num_bars = song.length();

    static Vec2 last_viewport_scroll = Vec2(0.0f, 0.0f);
    static Vec2 last_canvas_size = Vec2(0.0f, 0.0f);
    static int row_start = 0;
    static int row_end = 0;
    static int col_start = 0;
    static int col_end = 0;
    
    if (ImGui::Begin("Track Editor")) {
        // if song length or song channel count changed, then resize content size
        if (last_width != num_bars || last_height != num_channels) {
            Vec2 new_size = Vec2(num_bars, num_channels) * CELL_SIZE + Vec2(CHANNEL_COLUMN_WIDTH, 0.0f);
            ImGui::SetNextWindowContentSize(new_size);

            last_width = song.length();
            last_height = song.channels.size();
        }

        if (last_cursor_x != editor.selected_bar || last_cursor_y != editor.selected_channel) {
            int& cursor_x = editor.selected_bar;
            int& cursor_y = editor.selected_channel;

            Vec2 cursor_pos = Vec2(cursor_x * CELL_SIZE.x + CHANNEL_COLUMN_WIDTH, cursor_y * CELL_SIZE.y);
            Vec2 window_scroll = last_viewport_scroll;

            if (cursor_x >= col_end)
                window_scroll.x = cursor_pos.x + CELL_SIZE.x - last_canvas_size.x;
            else if (cursor_x <= col_start)
                window_scroll.x = cursor_pos.x - CHANNEL_COLUMN_WIDTH;

            if (cursor_y >= row_end)
                window_scroll.y = cursor_pos.y + CELL_SIZE.y - last_canvas_size.y;
            else if (cursor_y <= row_start)
                window_scroll.y = cursor_pos.y;

            // if desired scroll position has changed
            if (window_scroll != last_viewport_scroll)
                ImGui::SetNextWindowScroll(window_scroll);

            last_cursor_x = cursor_x;
            last_cursor_y = cursor_y;
        }

        ImGui::BeginChild("###track_editor_area", Vec2(-1, -1), false, ImGuiWindowFlags_HorizontalScrollbar);

        Vec2 canvas_size = (Vec2)ImGui::GetWindowSize() - Vec2(style.ScrollbarSize, style.ScrollbarSize);
        last_canvas_size = canvas_size;
        Vec2 canvas_p0 = ImGui::GetCursorScreenPos();
        Vec2 viewport_scroll = Vec2(ImGui::GetScrollX(), ImGui::GetScrollY());
        Vec2 mouse_pos = Vec2(io.MousePos) - canvas_p0 - Vec2(CHANNEL_COLUMN_WIDTH, 0.0f);
        Vec2 content_size = Vec2(num_bars, num_channels) * CELL_SIZE + Vec2(CHANNEL_COLUMN_WIDTH, 0.0f);

        int mouse_row = -1;
        int mouse_col = -1;

        ImGui::SetCursorPos(Vec2(CHANNEL_COLUMN_WIDTH, 0.0f));
        ImGui::InvisibleButton("track_editor_mouse_target", content_size - Vec2(CHANNEL_COLUMN_WIDTH, 0.0f));
        if (ImGui::IsItemHovered()) {
            mouse_row = (int)mouse_pos.y / CELL_SIZE.y;
            mouse_col = (int)mouse_pos.x / CELL_SIZE.x;

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                editor.selected_bar = mouse_col;
                editor.selected_channel = mouse_row;
            }
        }

        last_viewport_scroll = viewport_scroll;
        
        // use canvas for rendering
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        // visible bounds of viewport
        col_start = (int)viewport_scroll.x / CELL_SIZE.x;
        col_end = col_start + (int)(canvas_size.x - CHANNEL_COLUMN_WIDTH) / CELL_SIZE.x;
        row_start = (int)viewport_scroll.y / CELL_SIZE.y;
        row_end = row_start + (int)canvas_size.y / CELL_SIZE.y;
        
        char str_buf[8];

        // draw patterns
        Vec2 view_origin = (Vec2)ImGui::GetWindowPos() + Vec2(CHANNEL_COLUMN_WIDTH, 0.0f);
        ImGui::PushClipRect(
            view_origin,
            view_origin + content_size - Vec2(CHANNEL_COLUMN_WIDTH, 0.0f),
            true
        );

        for (int ch = row_start; ch < min(row_end + 2, num_channels); ch++) {
            for (int bar = col_start; bar < min(col_end + 2, num_bars); bar++) {
                Vec2 rect_pos = Vec2(canvas_p0.x + bar * CELL_SIZE.x + CELL_MARGIN + CHANNEL_COLUMN_WIDTH, canvas_p0.y + CELL_SIZE.y * ch + CELL_MARGIN);
                int pattern_num = song.channels[ch]->sequence[bar];
                bool is_selected = editor.selected_bar == bar && editor.selected_channel == ch;

                // draw cell background
                if (pattern_num > 0 || is_selected)
                    draw_list->AddRectFilled(
                        rect_pos,
                        Vec2(rect_pos.x + CELL_SIZE.x - CELL_MARGIN * 2, rect_pos.y + CELL_SIZE.y - CELL_MARGIN * 2),
                        is_selected ? editor.theme.get_channel_color(ch, true) : vec4_color(style.Colors[ImGuiCol_FrameBg])
                    );
                
                snprintf(str_buf, 8, "%i", pattern_num); // convert pattern_num to string (too lazy to figure out how to do it the C++ way)
                
                // draw pattern number
                draw_list->AddText(
                    rect_pos + (CELL_SIZE - Vec2(CELL_MARGIN, CELL_MARGIN) * 2.0f - ImGui::CalcTextSize(str_buf)) / 2.0f,
                    is_selected ? IM_COL32_BLACK : editor.theme.get_channel_color(ch, pattern_num > 0),
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
        Vec2 playhead_pos = canvas_p0 + Vec2(song_pos * CELL_SIZE.x + CHANNEL_COLUMN_WIDTH, viewport_scroll.y);
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
        for (int ch = row_start; ch < min(row_end + 2, num_channels); ch++) {
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

            auto& ch_dat = song.channels[ch];
            auto& vol_mod = ch_dat->vol_mod->module<audiomod::VolumeModule>();
            bool enable_mute = vol_mod.mute;
            bool enable_solo = ch_dat->solo;

            // mute button
            push_btn_disabled(style, !enable_mute);
            if (ImGui::SmallButton("M")) {
                vol_mod.mute = !enable_mute;
            }

            pop_btn_disabled();

            // solo button
            ImGui::SameLine();
            push_btn_disabled(style, !enable_solo);
            
            if (ImGui::SmallButton("S")) {
                ch_dat->solo = !ch_dat->solo;
            }

            pop_btn_disabled();
            
            ImGui::PopID();
        }

        ImGui::EndChild();
    } ImGui::End();

    // if selected pattern changed
    static int pattern_input = 0;
        
    // if one of these variables changes, then clear pattern_input
    static int last_selected_bar = editor.selected_bar;
    static int last_selected_ch = editor.selected_channel;

    if (last_selected_bar != editor.selected_bar || last_selected_ch != editor.selected_channel) {
        last_selected_bar = editor.selected_bar;
        last_selected_ch = editor.selected_channel;
        pattern_input = 0;
    }

    // pattern entering: number keys
    if (!io.WantTextInput)
    {
        for (int k = 0; k < 10; k++) {
            if (ImGui::IsKeyPressed((ImGuiKey)((int)ImGuiKey_0 + k))) {
                int& cell = song.channels[editor.selected_channel]->sequence[editor.selected_bar];
                int old_value = cell;

                pattern_input = (pattern_input * 10) + k;
                if (pattern_input > song.max_patterns()) pattern_input = k;

                if (pattern_input <= song.max_patterns())
                {
                    cell = pattern_input;

                    // register change
                    if (cell != old_value)
                        editor.push_change(new change::ChangeSequence(
                            editor.selected_channel,
                            editor.selected_bar,
                            old_value,
                            cell
                        ));
                }
            }
        }
    }
}