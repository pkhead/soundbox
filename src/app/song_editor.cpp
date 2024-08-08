#include <cfloat>
#include <imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <numutil.hpp>
#include "shortcuts.hpp"
#include "song.hpp"
#include "song_editor.hpp"
#include "../modules/internal/modules.hpp"
#include "app.hpp"
#include "theme.hpp"

using namespace sbox;

//////////////////////
// HELPER FUNCTIONS //
//////////////////////

inline unsigned int vec4_color(ImVec4 vec4)
{
    return ImGui::ColorConvertFloat4ToU32(vec4);
}

void push_btn_disabled(ImGuiStyle& style, bool is_disabled)
{
    ImVec4 btn_colors[3];
    btn_colors[0] = style.Colors[ImGuiCol_Button];
    btn_colors[1] = style.Colors[ImGuiCol_ButtonHovered];
    btn_colors[2] = style.Colors[ImGuiCol_ButtonActive];

    if (is_disabled)
    {
        for (int i = 0; i < 3; i++) {
            btn_colors[i].w *= 0.5f;
        }
    }

    ImGui::PushStyleColor(ImGuiCol_Button, btn_colors[0]);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btn_colors[1]);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, btn_colors[2]);
}

void pop_btn_disabled()
{
    ImGui::PopStyleColor(3);
}

//////////////////////
// SONG EDITOR MAIN //
//////////////////////

SongEditor::SongEditor(Song &song, ShortcutContext &shortcuts) :
    song(song),
    shortcuts(shortcuts)
{
    selected_channel = 0;
    quantization = 0.25f;
    note_preview = true;
    show_all_channels = true;
}

void SongEditor::draw()
{
    render_song_settings();
    render_channel_settings();
    render_track_editor();
    render_pattern_editor();
}

void SongEditor::play_note(unsigned int channel, unsigned int key, float velocity, float duration)
{
    logger::log_error("SongEditor::play_note: NOT IMPLEMENTED!");
}







void SongEditor::render_song_settings()
{
    if (ImGui::Begin("Song Settings")) {
        // song name input
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Name");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##song_name", &song.name);

        // play/prev/next
        if (ImGui::Button(song.is_playing ? "Pause##play_pause" : "Play##play_pause", ImVec2(-FLT_MIN, 0.0f)))
        {
            shortcuts.activate(ShortcutID::PLAY_PAUSE);
        }
                
        if (ImGui::Button("Prev", ImVec2(ImGui::GetWindowSize().x / -2.0f, 0.0f)))
            shortcuts.activate(ShortcutID::PLAYHEAD_PREV);

        ImGui::SameLine();
        if (ImGui::Button("Next", ImVec2(-1.0f, 0.0f)))
            shortcuts.activate(ShortcutID::PLAYHEAD_NEXT);
        
        // tempo
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Tempo");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::DragFloat("###song_tempo", &song.tempo, 1.0f, 0.0f, 5000.0f, "%.3f");
        if (song.tempo < 0) song.tempo = 0;

        /*{ // change detection
            float prev;
            if (change_detection(editor, song.tempo, &prev)) {
                editor.push_change(new change::ChangeSongTempo(ImGui::GetItemID(), prev, song.tempo));
            }
        }*/
        
        // TODO: controller/mod channels
        /*if (ImGui::BeginPopupContextItem()) {
            ImGui::Selectable("Add to selected modulator", false);
            ImGui::EndPopup();
        }*/

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

        // project notes
        ImGui::Text("Project Notes");
        ImGui::InputTextMultiline("###project_notes", &song.project_notes, ImVec2(-1.0f, ImGui::GetTextLineHeight() * 16.0f));
    } ImGui::End();
}








void SongEditor::render_channel_settings()
{
    static char char_buf[64];
    auto& cur_channel = song.get_channel(selected_channel);    

    if (ImGui::Begin("Channel Settings")) {
        // channel name
        ImGui::PushItemWidth(-1.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Name");
        ImGui::SameLine();
        ImGui::InputText("##channel_name", &cur_channel.name);

        auto &fader = cur_channel.output_fader;

        // volume slider
        {
            const float min_value = -25.0f;
            const float max_value = 25.0f;
            float gain = fader->control_get_value<float>(hosts::internal::FaderModule::FADER_CONTROL_GAIN);

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Volume");
            ImGui::SameLine();

            bool changed;
            if (gain <= min_value)
                changed = ImGui::SliderFloat("##channel_volume", &gain, min_value, max_value, "-inf dB", ImGuiSliderFlags_AlwaysClamp);
            else
                changed = ImGui::SliderFloat("##channel_volume", &gain, min_value, max_value, "%.2f dB", ImGuiSliderFlags_AlwaysClamp);

            if (ImGui::IsItemClicked(ImGuiMouseButton_Middle))
            {
                gain = 0.0f;
                changed = true;
            }

            if (changed)
            {
                if (gain <= min_value)
                    gain = -FLT_MAX;
                fader->control_set_value<float>(hosts::internal::FaderModule::FADER_CONTROL_GAIN, gain);
            }
        }

        // panning slider
        {
            float panning = fader->control_get_value<float>(hosts::internal::FaderModule::FADER_CONTROL_PAN);

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Panning");
            ImGui::SameLine();
            bool changed = ImGui::SliderFloat("##channel_panning", &panning, -1, 1, "%.2f");
            if (ImGui::IsItemClicked(ImGuiMouseButton_Middle))
            {
                panning = 0.0f;
                changed = true;
            }

            if (changed) fader->control_set_value<float>(hosts::internal::FaderModule::FADER_CONTROL_PAN, panning);
        }

        {
            // fx channel combobox
            ImGui::AlignTextToFramePadding();
            ImGui::Text("FX Channel");
            ImGui::SameLine();

            // write preview value
            uint cur_fx_index = cur_channel.effect_channel();
            auto& cur_fx_bus = song.get_effect_channel(cur_fx_index);
            snprintf(char_buf, 64, "%i - %s", cur_fx_index, cur_fx_bus.name.c_str());

            if (ImGui::BeginCombo("##channel_fx_target", char_buf))
            {
                // list potential targets
                for (size_t target_i = 0; target_i < song.effect_channel_count(); target_i++)
                {
                    auto& target_bus = song.get_effect_channel(target_i);

                    // write target bus name
                    snprintf(char_buf, 64, "%lu - %s", target_i, target_bus.name.c_str());

                    bool is_selected = target_i == cur_fx_index;
                    if (ImGui::Selectable(char_buf, is_selected))
                        song.route_instrument(selected_channel, target_i);

                    if (is_selected) ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }
        }

        ImGui::PopItemWidth();
        ImGui::NewLine();

        // load instrument
        /*ImGui::Text("Instrument: %s", cur_channel->synth_mod->module().name.c_str());
        if (ImGui::Button("Load...", ImVec2(ImGui::GetWindowSize().x / -2.0f, 0.0f)))
        {
            ImGui::OpenPopup("load_instrument");
        }
        ImGui::SameLine();

        if (ImGui::BeginPopup("load_instrument")) {
            const char* mod_id = module_selection_popup(editor, true);
            ImGui::EndPopup();

            if (mod_id)
            {
                try {
                    auto mod = audiomod::create_module(
                        mod_id,
                        editor.modctx,
                        editor.plugin_manager,
                        editor.song->work_scheduler
                    );

                    mod->module().song = editor.song.get();
                    mod->module().parent_name = cur_channel->name;
                    cur_channel->set_instrument(mod);
                } catch (plugins::module_create_error& err) {
                    show_status("Error: %s", err.what());
                }
            }
        }

        // edit loaded instrument
        if (ImGui::Button("Edit...", ImVec2(-1.0f, 0.0f)))
        {
            editor.toggle_module_interface(cur_channel->synth_mod);
        }

        EffectsInterfaceResult result;
        switch (effect_rack_ui(&editor, &cur_channel->effects_rack, &result, true))
        {
            case EffectsInterfaceAction::Add: {
                try {
                    auto mod = audiomod::create_module(
                        result.module_id,
                        editor.modctx,
                        editor.plugin_manager,
                        editor.song->work_scheduler
                    );
                    
                    mod->module().parent_name = cur_channel->name;
                    mod->module().song = &song;
                    cur_channel->effects_rack.insert(mod);

                    // register change
                    editor.push_change(new change::ChangeAddEffect(
                        editor.selected_channel,
                        change::FXRackTargetType::TargetChannel,
                        result.module_id
                    ));
                } catch (plugins::module_create_error& err) {
                    show_status("Error: %s", err.what());
                }
                
                break;
            }

            case EffectsInterfaceAction::Edit:
                editor.toggle_module_interface(cur_channel->effects_rack.modules[result.target_index]);
                break;

            case EffectsInterfaceAction::Delete: {
                // delete the selected module
                auto mod = cur_channel->effects_rack.remove(result.target_index);
                if (mod) {
                    // register change
                    editor.push_change(new change::ChangeRemoveEffect(
                        editor.selected_channel,
                        change::FXRackTargetType::TargetChannel,
                        result.target_index,
                        mod->module()
                    ));

                    editor.hide_module_interface(mod);
                }
                break;
            }

            case EffectsInterfaceAction::Swapped:
                editor.push_change(new change::ChangeSwapEffect(
                    editor.selected_channel,
                    change::FXRackTargetType::TargetChannel,
                    result.swap_start,
                    result.swap_end
                ));

                break;

            case EffectsInterfaceAction::SwapInstrument:
                std::cout << "TODO: swap instrument\n";
                break;

            case EffectsInterfaceAction::Nothing: break;
        }*/
        
    } ImGui::End();
}








void SongEditor::render_track_editor()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();

    assert(Application::instance != nullptr);
    Theme &app_theme = Application::instance->theme;

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

    static int last_cursor_x = selected_bar;
    static int last_cursor_y = selected_channel;
    static int last_width = song.length();
    static int last_height = song.channel_count();

    int num_channels = song.channel_count();
    int num_bars = song.length();

    static Vec2 last_viewport_scroll = Vec2(0.0f, 0.0f);
    static Vec2 last_canvas_size = Vec2(0.0f, 0.0f);
    static int row_start = 0;
    static int row_end = 0;
    static int col_start = 0;
    static int col_end = 0;

    if (shortcuts.is_activated(ShortcutID::CURSOR_LEFT))
    {
        selected_bar = ((int)selected_bar - 1) % song.length();
    }

    if (shortcuts.is_activated(ShortcutID::CURSOR_RIGHT))
    {
        selected_bar = (selected_bar + 1) % song.length();
    }

    if (shortcuts.is_activated(ShortcutID::CURSOR_UP))
    {
        selected_channel = ((int)selected_channel - 1) % song.channel_count();
    }

    if (shortcuts.is_activated(ShortcutID::CURSOR_DOWN))
    {
        selected_channel = (selected_channel + 1) % song.channel_count();
    }
    
    if (ImGui::Begin("Track Editor")) {
        // if song length or song channel count changed, then resize content size
        if (last_width != num_bars || last_height != num_channels) {
            Vec2 new_size = Vec2(num_bars, num_channels) * CELL_SIZE + Vec2(CHANNEL_COLUMN_WIDTH, 0.0f);
            ImGui::SetNextWindowContentSize(new_size);

            last_width = song.length();
            last_height = song.channel_count();
        }

        if (last_cursor_x != selected_bar || last_cursor_y != selected_channel) {
            uint& cursor_x = selected_bar;
            uint& cursor_y = selected_channel;

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
                selected_bar = mouse_col;
                selected_channel = mouse_row;
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

        for (int ch = row_start; ch < util::min(row_end + 2, num_channels); ch++) {
            for (int bar = col_start; bar < util::min(col_end + 2, num_bars); bar++) {
                Vec2 rect_pos = Vec2(canvas_p0.x + bar * CELL_SIZE.x + CELL_MARGIN + CHANNEL_COLUMN_WIDTH, canvas_p0.y + CELL_SIZE.y * ch + CELL_MARGIN);
                int pattern_num = song.get_channel(ch).sequence[bar];
                bool is_selected = selected_bar == bar && selected_channel == ch;

                // draw cell background
                if (pattern_num > 0 || is_selected)
                    draw_list->AddRectFilled(
                        rect_pos,
                        Vec2(rect_pos.x + CELL_SIZE.x - CELL_MARGIN * 2, rect_pos.y + CELL_SIZE.y - CELL_MARGIN * 2),
                        is_selected ? app_theme.get_channel_color(ch, true) : vec4_color(style.Colors[ImGuiCol_FrameBg])
                    );
                
                snprintf(str_buf, 8, "%i", pattern_num); // convert pattern_num to string (too lazy to figure out how to do it the C++ way)
                
                // draw pattern number
                draw_list->AddText(
                    rect_pos + (CELL_SIZE - Vec2(CELL_MARGIN, CELL_MARGIN) * 2.0f - ImGui::CalcTextSize(str_buf)) / 2.0f,
                    is_selected ? IM_COL32_BLACK : app_theme.get_channel_color(ch, pattern_num > 0),
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
        for (int ch = row_start; ch < util::min(row_end + 2, num_channels); ch++) {
            ImGui::PushID(ch);

            Vec2 row_start = Vec2(
                viewport_scroll.x,
                CELL_SIZE.y * ch + CELL_MARGIN + (CELL_SIZE.y - ImGui::GetTextLineHeightWithSpacing()) / 2.0f
            );
            
            ImGui::SetCursorPos(row_start);
            ImGui::Text("%s", song.get_channel(ch).name.c_str());
            
            ImGui::SetCursorPos(row_start + Vec2(
                CHANNEL_COLUMN_WIDTH - controls_width,
                0.0f)
            );

            auto& ch_dat = song.get_channel(ch);

            // mute button
            push_btn_disabled(style, !ch_dat.mute);
            if (ImGui::SmallButton("M")) {
                ch_dat.mute = !ch_dat.mute;
            }

            pop_btn_disabled();

            // solo button
            ImGui::SameLine();
            push_btn_disabled(style, !ch_dat.solo);
            
            if (ImGui::SmallButton("S")) {
                ch_dat.solo = !ch_dat.solo;
            }

            pop_btn_disabled();
            
            ImGui::PopID();
        }

        ImGui::EndChild();
    } ImGui::End();

    // if selected pattern changed
    static int pattern_input = 0;
        
    // if one of these variables changes, then clear pattern_input
    static int last_selected_bar = selected_bar;
    static int last_selected_ch = selected_channel;

    if (last_selected_bar != selected_bar || last_selected_ch != selected_channel) {
        last_selected_bar = selected_bar;
        last_selected_ch = selected_channel;
        pattern_input = 0;
    }

    // pattern entering: number keys
    if (!io.WantTextInput)
    {
        for (int k = 0; k < 10; k++) {
            if (ImGui::IsKeyPressed((ImGuiKey)((int)ImGuiKey_0 + k))) {
                uint& cell = song.get_channel(selected_channel).sequence[selected_bar];
                int old_value = cell;

                pattern_input = (pattern_input * 10) + k;
                if (pattern_input > song.max_patterns()) pattern_input = k;

                if (pattern_input <= song.max_patterns())
                {
                    cell = pattern_input;

                    /*// register change
                    if (cell != old_value)
                        editor.push_change(new change::ChangeSequence(
                            editor.selected_channel,
                            editor.selected_bar,
                            old_value,
                            cell
                        ));*/
                }
            }
        }
    }
}







constexpr float PIANO_KEY_VELOCITY = 0.8f;

void SongEditor::render_pattern_editor()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();
    Theme& theme = Application::instance->theme;
    //Tuning* tuning = song.tunings[song.selected_tuning];

    if (ImGui::Begin("Pattern Editor")) {
        // cell size including margin
        const Vec2 CELL_SIZE = Vec2(int((ImGui::GetTextLineHeight() + 2.0f) * 3.125f), int(ImGui::GetTextLineHeight() + 2.0f));
        // empty space inbetween cells
        const int CELL_MARGIN = 1;

        const float PIANO_KEY_WIDTH = int(ImGui::GetTextLineHeight() * 2.307f);


        Vec2 canvas_size = ImGui::GetContentRegionAvail();
        Vec2 offset = Vec2(canvas_size.x - (CELL_SIZE.x * song.beats_per_bar + PIANO_KEY_WIDTH + style.ScrollbarSize), 0) / 2.0f;

        // min step
        static int selected_step = 0;

        static const char* step_names[] = {
            "1/4",
            "1/8",
            "1/3",
            "1/6",
            "free",
        };

        static const float step_values[] = {
            0.25f,
            0.125f,
            1.0f / 3.0f,
            1.0f / 6.0f,
            0.0f
        };

        ImGui::SetCursorPos(Vec2(ImGui::GetCursorPos()) + offset + Vec2(0, 0));
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Rhythm");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetFrameHeight() * 2.7f);
        if (ImGui::BeginCombo("##pattern_editor_step", step_names[selected_step]))
        {
            for (int i = 0; i < 5; i++) {
                if (ImGui::Selectable(step_names[i], i == selected_step)) {
                    selected_step = i;
                    quantization = step_values[i];
                }

                if (i == selected_step) {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }
        
        // TODO: set key by right-clicking on a key in the piano rol
        if (true /*tuning->is_12edo*/)
        {
            ImGui::SameLine();
            ImGui::Text("Scale");
            
            // scale help
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 15.0f);
                ImGui::Text("Right-click on a piano key to set the root of the scale.");
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }

            // scale options
            ImGui::SetNextItemWidth(ImGui::GetFrameHeight() * 8.0f);
            ImGui::SameLine();
            if (ImGui::BeginCombo("##pattern_editor_scale", "Ionian (major)")) {
                ImGui::Selectable("Ionian (major)", true);
                ImGui::SetItemDefaultFocus();
                ImGui::Selectable("Aeolian (minor)", false);

                ImGui::EndCombo();
            }
        }

        ImGui::NewLine();

        float min_step = quantization;

        static int scroll = 96;
        static const char* KEY_NAMES[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
        static const bool ACCIDENTAL[12] = {false, true, false, true, false, false, true, false, true, false, true, false};
        static char key_name[8];
        static float cursor_note_length = 1.0f;
        static float desired_cursor_len = cursor_note_length;

        static float mouse_start = 0; // the mouse x when the drag started
        static Note* note_hovered = nullptr; // the note currently being hovered over
        static Note* selected_note = nullptr; // the note currently being dragged
        static Pattern* note_pattern = nullptr; // the pattern of selected_note
        static float note_anchor; // selected_note's time position when the drag started
        static float note_start_length; // selected_note's length when the drag started
        static Note old_note_data; // data of selected note before change
        static bool from_null_pattern = false; // if added note from a null pattern
        static int old_max_patterns = song.max_patterns(); // max patterns from note was added
        
        // this variable are used to determine whether the user simply clicked on a note
        // to delete it
        static Vec2 mouse_screen_start;
        static bool did_mouse_move;
        static bool play_key = false;
        static int played_key = 0;
        static int prev_mouse_cy = 0;

        static bool is_adding_note;
        enum DragMode {
            FromLeft,
            FromRight,
            Any
        } static note_drag_mode;

        // center viewport
        ImGui::SetCursorPos(Vec2(ImGui::GetCursorPos()) + offset + Vec2(0, -CELL_SIZE.y));

        // create scrollable area
        static constexpr int VIEW_RANGE = 12 * 8 + 1;
        ImGui::BeginChild("###pattern_editor_notes", Vec2(CELL_SIZE.x * song.beats_per_bar + PIANO_KEY_WIDTH + style.ScrollbarSize, -1.0f));
        canvas_size = ImGui::GetContentRegionAvail();

        static bool child_created = false;
        if (!child_created) {
            child_created = true;
            ImGui::SetScrollY(CELL_SIZE.y * (VIEW_RANGE - 60 - 1));
        }

        Vec2 canvas_p0 = ImGui::GetCursorScreenPos();
        Vec2 canvas_p1 = canvas_p0 + canvas_size;
        Vec2 draw_origin = canvas_p0;

        // define interactable area
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        Vec2 button_size = Vec2(CELL_SIZE.x * song.beats_per_bar + PIANO_KEY_WIDTH, canvas_size.y + style.WindowPadding.y);
        if (button_size.x <= 0.0f) button_size.x = 1.0f;
        if (button_size.y <= 0.0f) button_size.y = 1.0f;

        ImGui::InvisibleButton("pattern_editor_click_area",
            Vec2(-1.0f, CELL_SIZE.y * VIEW_RANGE),
            ImGuiButtonFlags_MouseButtonLeft
        );

        Vec2 viewport_scroll = (Vec2)ImGui::GetWindowPos() - canvas_p0;

        static InstrumentChannel* prev_channel = nullptr;
        
        // get data for the currently selected channel
        InstrumentChannel &cur_channel = song.get_channel(selected_channel);
        int pattern_id = cur_channel.sequence[selected_bar];
        Pattern* selected_pattern = nullptr;

        if (pattern_id > 0) {
            selected_pattern = cur_channel.patterns[pattern_id - 1].get();
        }

        Vec2 mouse_pos = Vec2(io.MousePos) - canvas_p0;
        float mouse_px = -1.0f; // cell position of mouse
        float mouse_cx = -1.0f; // mouse_px is in the center of the mouse cursor note
        int mouse_cy = (int)mouse_pos.y / CELL_SIZE.y;

        if (mouse_cy < 0) mouse_cy = 0;
        
        // calculate mouse grid position
        if (true) {
            mouse_px = (mouse_pos.x - PIANO_KEY_WIDTH) / CELL_SIZE.x;
            mouse_cx = mouse_px - desired_cursor_len / 2.0f;
            if (min_step > 0) mouse_cx = floorf(mouse_cx / min_step + 0.5f) * min_step;

            // prevent collision with mouse cursor note & other notes
            float min = 0;
            float max = song.beats_per_bar;

            if (selected_note == nullptr && selected_pattern != nullptr) {
                for (Note& note : selected_pattern->notes) {
                    if (scroll - mouse_cy == note.key) {
                        // if mouse is on right side of this note
                        if (mouse_px > note.time + note.length / 2.0f) {
                            if (note.time + note.length > min) min = note.time + note.length;
                        }

                        // if mouse is on left side of this note
                        if (mouse_px < note.time + note.length / 2.0f) {
                            if (note.time < max) max = note.time;
                        }
                    }
                }
            }

            // prevent it from going off the edges of the screen
            cursor_note_length = desired_cursor_len;

            // if space is too small for note cursor to fit in, resize it
            if (max - min < desired_cursor_len) {
                mouse_cx = min;
                cursor_note_length = max - min;
            } else {
                if (mouse_cx < min) mouse_cx = min;
                if (mouse_cx + cursor_note_length > max) mouse_cx = max - cursor_note_length;
            }
        }

        // deselect note if selected pattern changes
        if (selected_pattern != note_pattern) {
            selected_note = nullptr;
            note_pattern = selected_pattern;
        }

        // detect which note the user is hoving over
        note_hovered = nullptr;
        
        if (selected_pattern != nullptr) {
            for (Note& note : selected_pattern->notes) {
                if (
                    (scroll - mouse_cy) == note.key &&
                    mouse_px >= note.time &&
                    mouse_px < note.time + note.length
                ) {
                    note_hovered = &note;
                    break;
                }
            }
        }

        // if area is clicked
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            // if on note area, add/resize a note
            if (mouse_px >= 0.0f) {
                mouse_screen_start = mouse_pos;
                did_mouse_move = false;
                
                // add note on click
                if (ImGui::IsItemHovered()) {
                    // if selected null pattern, create a new pattern
                    // or reuse an empty one
                    from_null_pattern = selected_pattern == nullptr;
                    old_max_patterns = song.max_patterns();

                    if (selected_pattern == nullptr)
                    {
                        int pattern = song.new_pattern(selected_channel);
                        cur_channel.sequence[selected_bar] = pattern;
                        selected_pattern = cur_channel.patterns[pattern - 1].get();
                    }
                    
                    mouse_start = mouse_px;

                    if (note_hovered) {
                        is_adding_note = false;
                        selected_note = note_hovered;
                        old_note_data = *selected_note;

                        if (mouse_px > selected_note->time + selected_note->length / 2.0f) {
                            note_drag_mode = DragMode::FromRight;
                            note_anchor = selected_note->time;
                            note_start_length = selected_note->length;
                        } else {
                            note_drag_mode = DragMode::FromLeft;
                            note_anchor = selected_note->time + selected_note->length;
                            note_start_length = -selected_note->length;
                        }
                    } else {
                        int key = scroll - mouse_cy;

                        is_adding_note = true;
                        note_drag_mode = DragMode::Any;

                        selected_note = &selected_pattern->add_note(mouse_cx, key, cursor_note_length);
                        
                        note_anchor = mouse_cx;
                        note_start_length = cursor_note_length;

                        // Preview Added Note
                        if (!song.is_playing && note_preview)
                            play_note(selected_channel, key, PIANO_KEY_VELOCITY, 0.2f);
                    }

                    note_pattern = selected_pattern;
                }
            }

            // if in piano area, play a note
            else {
                // stop already currently playing note
                if (play_key && song.is_note_playable(played_key)) {
                    // turn off old note
                    cur_channel.send_midi(midi::note_off(0, played_key, PIANO_KEY_VELOCITY));
                }

                play_key = true;
                int key = scroll - mouse_cy;
                played_key = key;

                if (song.is_note_playable(key)) {
                    cur_channel.send_midi(midi::note_on(0, played_key, PIANO_KEY_VELOCITY));
                }
            }
        }

        if (ImGui::IsItemActive()) {
            // detect if mouse had moved while down
            if (!did_mouse_move) {
                if ((mouse_pos - mouse_screen_start).magn_sq() > 2*2) {
                    did_mouse_move = true;
                }
            
            // piano key glissando
            } else if (play_key) {
                if (prev_mouse_cy != mouse_cy) {
                    if (song.is_note_playable(played_key)) {
                        // turn off old note
                        cur_channel.send_midi(midi::note_off(0, played_key, PIANO_KEY_VELOCITY));

                        // turn on new note
                        played_key = scroll - mouse_cy;
                        cur_channel.send_midi(midi::note_on(0, played_key, PIANO_KEY_VELOCITY));
                    }
                }
            }

            // mouse note dragging
            if (selected_note != nullptr && did_mouse_move) {
                float new_len = (mouse_px - mouse_start) + note_start_length;
                if (min_step > 0) {
                    float note_end = selected_note->time + new_len;
                    note_end = floorf(note_end / min_step + 0.5f) * min_step;
                    new_len = note_end - selected_note->time;
                }

                if (note_drag_mode == DragMode::FromRight || (note_drag_mode == DragMode::Any && new_len >= 0)) {
                    if (new_len < 0) {
                        selected_note->length = 0.0f;
                    } else {
                        if (new_len < min_step) new_len = min_step;

                        selected_note->time = note_anchor;
                        selected_note->length = new_len;

                        // prevent it from going off the right side of the viewport
                        if (selected_note->time + selected_note->length > song.beats_per_bar) {
                            selected_note->length = (float)song.beats_per_bar - selected_note->time;
                        }

                        // prevent it from overlapping with other notes
                        for (Note& note : note_pattern->notes) {
                            if (note.key == selected_note->key && selected_note->time + selected_note->length > note.time && selected_note->time < note.time) {
                                selected_note->length = note.time - selected_note->time;
                            }
                        }
                    }
                    
                } else {
                    if (new_len > 0) {
                        selected_note->length = 0.0f;
                        selected_note->time = note_anchor;
                    } else {
                        if (new_len > -min_step) new_len = -min_step;

                        selected_note->time = note_anchor + new_len;
                        selected_note->length = -new_len;
                        
                        //selected_note->time = floorf(selected_note->time / min_step + 0.5f) * min_step;
                        //selected_note->length = floorf(selected_note->length / min_step + 0.5f) * min_step;

                        // prevent it from going off the left side of the viewport
                        if (selected_note->time < 0) {
                            selected_note->time = 0;
                            selected_note->length = note_anchor;
                        }

                        // prevent it from overlapping with other notes
                        for (Note& note : note_pattern->notes) {
                            if (
                                note.key == selected_note->key &&
                                selected_note->time < note.time + note.length &&
                                selected_note->time + selected_note->length > note.time + note.length
                            ) {
                                selected_note->time = note.time + note.length;
                                selected_note->length = note_anchor - selected_note->time;
                            }
                        }
                    }
                }
            }
        }

        // if selected channel changed, turn off currently playing note
        if (&cur_channel != prev_channel) {
            if (prev_channel != nullptr && play_key) {
                if (song.is_note_playable(played_key)) {
                    prev_channel->send_midi(midi::note_off(0, played_key, PIANO_KEY_VELOCITY));
                }

                play_key = false;
            }

            prev_channel = &cur_channel;
        }

        if (ImGui::IsItemDeactivated()) {
            if (selected_note != nullptr) {
                // if mouse hadn't moved 5 pixels since the click was started, remove the selected note
                // or if note len == 0, remove the note
                if (selected_note->length == 0 || (!is_adding_note && !did_mouse_move)) {
                    for (auto it = note_pattern->notes.begin(); it != note_pattern->notes.end(); it++) {
                        if (&*it == selected_note) {
                            // register change
                            /*editor.push_change(new change::ChangeRemoveNote(
                                editor.selected_channel,
                                editor.selected_bar,
                                *it
                            ));*/

                            note_pattern->notes.erase(it);
                            break;
                        }
                    }
                } else {
                    // register change
                    if (is_adding_note)
                    {
                        /*editor.push_change(new change::ChangeAddNote(
                            editor.selected_channel,
                            editor.selected_bar,
                            song, from_null_pattern, old_max_patterns,
                            *selected_note
                        ));*/
                    }
                    else if (old_note_data != *selected_note)
                    {
                        /*editor.push_change(new change::ChangeNote(
                            editor.selected_channel,
                            editor.selected_bar,
                            old_note_data,
                            *selected_note
                        ));*/
                    }

                    desired_cursor_len = selected_note->length;
                }

                note_pattern = nullptr;
                selected_note = nullptr;
            }

            // if currently playing a note
            if (play_key) {
                // turn off old note
                if (song.is_note_playable(played_key)) {
                    cur_channel.send_midi(midi::note_off(0, played_key, PIANO_KEY_VELOCITY));
                }

                play_key = false;
            }

            is_adding_note = false;
        }

        // draw cells
        int row;
        for (int i = 0; i < (int)canvas_size.y / CELL_SIZE.y + 2; i++) {
            row = i + (viewport_scroll.y / CELL_SIZE.y);
            
            int key = scroll - row;
            
            if (key < 0) continue;

            //Tuning::KeyInfoStruct& tuning_info = tuning->key_info[key];

            // draw piano key
            Vec2 piano_rect_pos = draw_origin + CELL_SIZE * Vec2(0, row);

            ImU32 key_color;

            if (true /*tuning->is_12edo*/)
            {
                // color based on accidental/octave
                key_color =
                    key % 12 == 0 ?
                        vec4_color(theme.get_custom_color(CustomColor::PianoKeyOctave)) : // octave
                        ACCIDENTAL[key % 12] ? vec4_color(theme.get_custom_color(CustomColor::PianoKeyAccidental)) : // accidental
                        vec4_color(theme.get_custom_color(CustomColor::PianoKey)); // default
            }
            else
            {
                //key_color = tuning->key_info[key].key_color;
            }

            draw_list->AddRectFilled(
                piano_rect_pos + Vec2(CELL_MARGIN, CELL_MARGIN),
                piano_rect_pos + Vec2(PIANO_KEY_WIDTH - CELL_MARGIN * 2, CELL_SIZE.y - CELL_MARGIN * 2),
                key_color
            );

            // get key name
            float text_size;

            if (true /*tuning->is_12edo*/)
            {
                int key_mod = (key % 12 + 12) % 12;
                strncpy(key_name, KEY_NAMES[key_mod], 8);

                // if key is C, then add the octave number
                if (key % 12 == 0) snprintf(key_name + 1, 7, "%i", key / 12);
            }
            else if (key >= 0)
            {
                // is fifth
                /*if (tuning_info.is_fifth)
                    strncpy(key_name, "3/2", 8);

                // is octave
                else if (tuning_info.is_octave)
                    snprintf(key_name, 8, "%i", tuning_info.octave_number);

                // no name
                else strncpy(key_name, "", 8);*/
            }

            // draw key name
            text_size = ImGui::GetFontSize();

            draw_list->AddText(
                piano_rect_pos + Vec2(5, (CELL_SIZE.y - text_size) / 2.0f),
                IM_COL32_WHITE,
                key_name
            );

            bool is_octave = key % 12 == 0;
            bool is_fifth = key % 12 == 7;
            
            ImU32 row_color =
                is_octave ? vec4_color(theme.get_custom_color(CustomColor::OctaveRow)) : // highlight each octave
                is_fifth ? vec4_color(theme.get_custom_color(CustomColor::FifthRow)) : // highlight each fifth
                vec4_color(style.Colors[ImGuiCol_FrameBg]); // default color

            // draw cells in this row
            for (int col = 0; col < 8; col++) {
                Vec2 cell_pos = draw_origin + CELL_SIZE * Vec2(col, row) + Vec2(PIANO_KEY_WIDTH, 0);
                Vec2 rect_pos = cell_pos + Vec2(CELL_MARGIN, CELL_MARGIN);

                draw_list->AddRectFilled(rect_pos, rect_pos + CELL_SIZE - Vec2(CELL_MARGIN, CELL_MARGIN) * 2.0f, row_color);
            }
        }

        // draw pattern notes from other channels
        if (show_all_channels)
        {
            for (int ch_i = 0; ch_i < song.channel_count(); ch_i++)
            {
                // don't redraw selected pattern
                if (ch_i == selected_channel) continue;
                auto &channel = song.get_channel(ch_i);

                // get pattern
                int p_id = channel.sequence[selected_bar];
                if (p_id == 0) continue; // don't draw null pattern
                auto& pattern = channel.patterns[p_id - 1];

                // draw notes in pattern
                for (Note& note : pattern->notes)
                {
                    Vec2 cell_pos = draw_origin + CELL_SIZE * Vec2(note.time, scroll - note.key) + Vec2(PIANO_KEY_WIDTH, 0);
                    Vec2 rect_pos = cell_pos + Vec2(CELL_MARGIN, 5);

                    draw_list->AddRectFilled(
                        rect_pos, 
                        rect_pos + CELL_SIZE * Vec2(note.length, 1.0f) - Vec2(CELL_MARGIN, 0) * 2.0f - Vec2(0, 10),
                        theme.get_channel_color(ch_i, false)
                    );
                }
            }
        }

        if (selected_pattern != nullptr) {
            // draw notes of currently selected pattern
            for (Note& note : selected_pattern->notes) {
                Vec2 cell_pos = draw_origin + CELL_SIZE * Vec2(note.time, scroll - note.key) + Vec2(PIANO_KEY_WIDTH, 0);
                Vec2 rect_pos = cell_pos + Vec2(CELL_MARGIN, 0);

                draw_list->AddRectFilled(
                    rect_pos, 
                    rect_pos + CELL_SIZE * Vec2(note.length, 1.0f) - Vec2(CELL_MARGIN, 0) * 2.0f,
                    theme.get_channel_color(selected_channel, true)
                );
            }
        }

        // draw rectangle stroke at mouse position
        if (ImGui::IsItemHovered() && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (mouse_pos.x > PIANO_KEY_WIDTH) {
                float cx = mouse_cx;
                int cy = mouse_cy;
                float len = cursor_note_length;

                if (note_hovered != nullptr) {
                    cx = note_hovered->time;
                    cy = scroll - note_hovered->key;
                    len = note_hovered->length;
                }

                Vec2 rect_pos = Vec2(draw_origin.x + PIANO_KEY_WIDTH + CELL_SIZE.x * cx, draw_origin.y + CELL_SIZE.y * cy);
                
                draw_list->AddRect(
                    rect_pos, rect_pos + CELL_SIZE * Vec2(len, 1.0f),
                    vec4_color(style.Colors[ImGuiCol_Text])
                );
            } else {
                Vec2 rect_pos = Vec2(draw_origin.x, draw_origin.y + CELL_SIZE.y * mouse_cy);
                
                draw_list->AddRect(
                    rect_pos, rect_pos + Vec2(PIANO_KEY_WIDTH, CELL_SIZE.y),
                    vec4_color(style.Colors[ImGuiCol_Text])
                );
            }
        }

        // draw playhead
        if (song.is_playing && cur_channel.sequence[song.bar_position] == pattern_id) {
            Vec2 playhead_pos = draw_origin + Vec2(PIANO_KEY_WIDTH + fmodf(song.position, song.beats_per_bar) * CELL_SIZE.x, viewport_scroll.y);
            draw_list->AddRectFilled(playhead_pos, playhead_pos + Vec2(1.0f, canvas_size.y + style.WindowPadding.y * 2.0f), vec4_color(style.Colors[ImGuiCol_Text]));
        }

        prev_mouse_cy = mouse_cy;

        ImGui::EndChild();
    } ImGui::End();
}