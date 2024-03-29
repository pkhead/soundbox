#include <math.h>

#include <imgui.h>
#include "../editor/change_history.h"
#include "../editor/theme.h"
#include "../song.h"
#include "ui.h"

using namespace ui;

constexpr float PIANO_KEY_VELOCITY = 0.8f;

void ui::render_pattern_editor(SongEditor &editor)
{
    ImGuiIO& io = ImGui::GetIO();
    Song& song = *editor.song;
    ImGuiStyle& style = ImGui::GetStyle();
    Theme& theme = editor.theme;
    Tuning* tuning = song.tunings[song.selected_tuning];

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
                    editor.quantization = step_values[i];
                }

                if (i == selected_step) {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }
        
        // TODO: set key by right-clicking on a key in the piano rol
        if (tuning->is_12edo)
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

        float min_step = editor.quantization;

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

        static Channel* prev_channel = nullptr;
        
        // get data for the currently selected channel
        Channel* selected_channel = song.channels[editor.selected_channel].get();
        int pattern_id = selected_channel->sequence[editor.selected_bar] - 1;
        Pattern* selected_pattern = nullptr;

        if (pattern_id >= 0) {
            selected_pattern = selected_channel->patterns[selected_channel->sequence[editor.selected_bar] - 1].get();
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
                        int pattern = song.new_pattern(editor.selected_channel);
                        selected_channel->sequence[editor.selected_bar] = pattern + 1;
                        selected_pattern = selected_channel->patterns[pattern].get();
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
                        if (!song.is_playing && editor.note_preview)
                            editor.play_note(editor.selected_channel, key, PIANO_KEY_VELOCITY, 0.2f);
                    }

                    note_pattern = selected_pattern;
                }
            }

            // if in piano area, play a note
            else {
                // stop already currently playing note
                if (play_key && song.is_note_playable(played_key)) {
                    // turn off old note
                    selected_channel->synth_mod->module().queue_event(audiomod::NoteEvent {
                        audiomod::NoteEventKind::NoteOff,
                        played_key,
                        PIANO_KEY_VELOCITY
                    });
                }

                play_key = true;
                int key = scroll - mouse_cy;
                played_key = key;

                if (song.is_note_playable(key)) {
                    selected_channel->synth_mod->module().queue_event(audiomod::NoteEvent {
                        audiomod::NoteEventKind::NoteOn,
                        key,
                        PIANO_KEY_VELOCITY
                    });
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
                    // turn off old note
                    if (song.is_note_playable(played_key)) {
                        selected_channel->synth_mod->module().queue_event(audiomod::NoteEvent {
                            audiomod::NoteEventKind::NoteOff,
                            played_key,
                            PIANO_KEY_VELOCITY
                        });

                        // turn on new note
                        played_key = scroll - mouse_cy;
                        selected_channel->synth_mod->module().queue_event(audiomod::NoteEvent {
                            audiomod::NoteEventKind::NoteOn,
                            played_key,
                            PIANO_KEY_VELOCITY
                        });
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
        if (selected_channel != prev_channel) {
            if (prev_channel != nullptr && play_key) {
                if (song.is_note_playable(played_key)) {
                    prev_channel->synth_mod->module().queue_event(audiomod::NoteEvent {
                        audiomod::NoteEventKind::NoteOff,
                        played_key,
                        PIANO_KEY_VELOCITY
                    });
                }

                play_key = false;
            }

            prev_channel = selected_channel;
        }

        if (ImGui::IsItemDeactivated()) {
            if (selected_note != nullptr) {
                // if mouse hadn't moved 5 pixels since the click was started, remove the selected note
                // or if note len == 0, remove the note
                if (selected_note->length == 0 || (!is_adding_note && !did_mouse_move)) {
                    for (auto it = note_pattern->notes.begin(); it != note_pattern->notes.end(); it++) {
                        if (&*it == selected_note) {
                            // register change
                            editor.push_change(new change::ChangeRemoveNote(
                                editor.selected_channel,
                                editor.selected_bar,
                                *it
                            ));

                            note_pattern->notes.erase(it);
                            break;
                        }
                    }
                } else {
                    // register change
                    if (is_adding_note)
                    {
                        editor.push_change(new change::ChangeAddNote(
                            editor.selected_channel,
                            editor.selected_bar,
                            song, from_null_pattern, old_max_patterns,
                            *selected_note
                        ));
                    }
                    else if (old_note_data != *selected_note)
                    {
                        editor.push_change(new change::ChangeNote(
                            editor.selected_channel,
                            editor.selected_bar,
                            old_note_data,
                            *selected_note
                        ));
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
                    selected_channel->synth_mod->module().queue_event(audiomod::NoteEvent {
                        audiomod::NoteEventKind::NoteOff,
                        played_key,
                        PIANO_KEY_VELOCITY
                    });
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

            Tuning::KeyInfoStruct& tuning_info = tuning->key_info[key];

            // draw piano key
            Vec2 piano_rect_pos = draw_origin + CELL_SIZE * Vec2(0, row);

            ImU32 key_color;

            if (tuning->is_12edo)
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
                key_color = tuning->key_info[key].key_color;
            }

            draw_list->AddRectFilled(
                piano_rect_pos + Vec2(CELL_MARGIN, CELL_MARGIN),
                piano_rect_pos + Vec2(PIANO_KEY_WIDTH - CELL_MARGIN * 2, CELL_SIZE.y - CELL_MARGIN * 2),
                key_color
            );

            // get key name
            float text_size;

            if (tuning->is_12edo)
            {
                int key_mod = (key % 12 + 12) % 12;
                strncpy(key_name, KEY_NAMES[key_mod], 8);

                // if key is C, then add the octave number
                if (key % 12 == 0) snprintf(key_name + 1, 7, "%i", key / 12);
            }
            else if (key >= 0)
            {
                // is fifth
                if (tuning_info.is_fifth)
                    strncpy(key_name, "3/2", 8);

                // is octave
                else if (tuning_info.is_octave)
                    snprintf(key_name, 8, "%i", tuning_info.octave_number);

                // no name
                else strncpy(key_name, "", 8);
            }

            // draw key name
            text_size = ImGui::GetFontSize();

            draw_list->AddText(
                piano_rect_pos + Vec2(5, (CELL_SIZE.y - text_size) / 2.0f),
                IM_COL32_WHITE,
                key_name
            );
            
            ImU32 row_color =
                tuning_info.is_octave ? vec4_color(theme.get_custom_color(CustomColor::OctaveRow)) : // highlight each octave
                tuning_info.is_fifth ? vec4_color(theme.get_custom_color(CustomColor::FifthRow)) : // highlight each fifth
                vec4_color(style.Colors[ImGuiCol_FrameBg]); // default color

            // draw cells in this row
            for (int col = 0; col < 8; col++) {
                Vec2 cell_pos = draw_origin + CELL_SIZE * Vec2(col, row) + Vec2(PIANO_KEY_WIDTH, 0);
                Vec2 rect_pos = cell_pos + Vec2(CELL_MARGIN, CELL_MARGIN);

                draw_list->AddRectFilled(rect_pos, rect_pos + CELL_SIZE - Vec2(CELL_MARGIN, CELL_MARGIN) * 2.0f, row_color);
            }
        }

        // draw pattern notes from other channels
        if (editor.show_all_channels)
        {
            for (int ch_i = 0; ch_i < song.channels.size(); ch_i++)
            {
                // don't redraw selected pattern
                if (ch_i == editor.selected_channel) continue;
                auto& channel = song.channels[ch_i];

                // get pattern
                int p_id = channel->sequence[editor.selected_bar];
                if (p_id == 0) continue; // don't draw null pattern
                auto& pattern = channel->patterns[p_id - 1];

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
                    theme.get_channel_color(editor.selected_channel, true)
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
        if (song.is_playing && selected_channel->sequence[song.bar_position] - 1 == pattern_id) {
            Vec2 playhead_pos = draw_origin + Vec2(PIANO_KEY_WIDTH + fmodf(song.position, song.beats_per_bar) * CELL_SIZE.x, viewport_scroll.y);
            draw_list->AddRectFilled(playhead_pos, playhead_pos + Vec2(1.0f, canvas_size.y + style.WindowPadding.y * 2.0f), vec4_color(style.Colors[ImGuiCol_Text]));
        }

        prev_mouse_cy = mouse_cy;

        ImGui::EndChild();
    } ImGui::End();
}