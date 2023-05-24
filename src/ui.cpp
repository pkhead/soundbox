#include <math.h>
#include "ui.h"

#define IM_RGB32(R, G, B) IM_COL32(R, G, B, 255)

namespace Colors {
    constexpr size_t channel_num = 4;
    ImU32 channel[channel_num][2] = {
        // { dim, bright }
        { IM_RGB32(187, 17, 17), IM_RGB32(255, 87, 87) },
        { IM_RGB32(187, 128, 17), IM_RGB32(255, 197, 89) },
        { IM_RGB32(136, 187, 17), IM_RGB32(205, 255, 89) },
        { IM_RGB32(25, 187, 17), IM_RGB32(97, 255, 89) },
    };
}

template <typename T>
static inline T max(T a, T b) {
    return a > b ? a : b;
}

template <typename T>
static inline T min(T a, T b) {
    return a < b ? a : b;
}

struct Vec2 {
    float x, y;
    constexpr Vec2() : x(0.0f), y(0.0f) {}
    constexpr Vec2(float _x, float _y) : x(_x), y(_y) {}
    Vec2(const ImVec2& src): x(src.x), y(src.y) {}

    inline Vec2 operator+(const Vec2& other) const {
        return Vec2(x + other.x, y + other.y);
    }
    inline Vec2 operator-(const Vec2& other) const {
        return Vec2(x - other.x, y - other.y);
    }
    inline Vec2 operator*(const Vec2& other) const {
        return Vec2(x * other.x, y * other.y);
    }
    inline Vec2 operator/(const Vec2& other) const {
        return Vec2(x / other.x, y / other.y);
    }

    inline float magn_sq() const {
        return x * x + y * y; 
    }

    inline float magn() const {
        return sqrtf(x * x + y * y);
    }

    template <typename T>
    inline Vec2 operator*(const T& scalar) const {
        return Vec2(x * scalar, y * scalar);
    }

    template <typename T>
    inline Vec2 operator/(const T& scalar) const {
        return Vec2(x / scalar, y / scalar);
    }

    inline operator ImVec2() const { return ImVec2(x, y); }
};

UserAction::UserAction(const std::string& action_name, uint8_t mod, ImGuiKey key) {
    name = action_name;
    set_keybind(mod, key);
}

void UserAction::set_keybind(uint8_t mod, ImGuiKey key) {
    this->modifiers = mod;
    this->key = key;

    // set combo string
    combo.clear();

    if ((mod & USERMOD_SHIFT) != 0) combo += "Shift+";
    if ((mod & USERMOD_CTRL) != 0) combo += "Ctrl+";
    if ((mod & USERMOD_ALT) != 0) combo += "Alt+";
    combo += ImGui::GetKeyName(key);
}

UserActionList::UserActionList() {
    add_action("song_save", USERMOD_CTRL, ImGuiKey_S);
    add_action("song_new", USERMOD_CTRL, ImGuiKey_N);
    add_action("song_save_as", USERMOD_CTRL | USERMOD_SHIFT, ImGuiKey_S);
    add_action("song_open", USERMOD_CTRL, ImGuiKey_O);
    add_action("song_play_pause", 0, ImGuiKey_Space);
    add_action("song_prev_bar", 0, ImGuiKey_LeftBracket);
    add_action("song_next_bar", 0, ImGuiKey_RightBracket);
    add_action("song_quit", 0, ImGuiKey_None);

    add_action("undo", USERMOD_CTRL, ImGuiKey_Z);
#ifdef _WIN32
    add_action("redo", USERMOD_CTRL, ImGuiKey_Y);
#else
    add_action("redo", USERMOD_CTRL | MOD_SHIFT, ImGuiKey_Z);
#endif
    add_action("copy", USERMOD_CTRL, ImGuiKey_C);
    add_action("paste", USERMOD_CTRL, ImGuiKey_V);
}

void UserActionList::add_action(const std::string& action_name, uint8_t mod, ImGuiKey key) {
    actions.push_back(UserAction(action_name, mod, key));
}

void UserActionList::fire(const std::string& action_name) const {
    for (const UserAction& action : actions) {
        if (action.name == action_name) {
            action.callback();
            break;
        }
    }
}

void UserActionList::set_keybind(const std::string& action_name, uint8_t mod, ImGuiKey key) {
    for (UserAction& action : actions) {
        if (action.name == action_name) {
            action.set_keybind(mod, key);
            break;
        }
    }
}

void UserActionList::set_callback(const std::string& action_name, std::function<void()> callback) {
    for (UserAction& action : actions) {
        if (action.name == action_name) {
            action.callback = callback;
            break;
        }
    }
}

const char* UserActionList::combo_str(const std::string& action_name) const {
    for (const UserAction& action : actions) {
        if (action.name == action_name) {
            return action.combo.c_str();
            break;
        }
    }

    return "";
}

















bool show_demo_window;

void compute_imgui(ImGuiIO& io, Song& song, UserActionList& user_actions) {
    ImGuiStyle& style = ImGui::GetStyle();

    static int bus_index = 0;

    static const char* bus_names[] = {
        "0 - master",
        "1 - bus 1",
        "2 - bus 2",
        "3 - drums",
    };

    ImGui::NewFrame();

    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_AutoHideTabBar);
    if (show_demo_window) ImGui::ShowDemoWindow();

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New", user_actions.combo_str("song_new"))) user_actions.fire("song_new");
            if (ImGui::MenuItem("Open", user_actions.combo_str("song_open"))) user_actions.fire("song_open");
            if (ImGui::MenuItem("Save", user_actions.combo_str("song_save"))) user_actions.fire("song_save");
            if (ImGui::MenuItem("Save As...", user_actions.combo_str("song_save_as"))) user_actions.fire("song_save_as");
            ImGui::Separator();
            ImGui::MenuItem("Export...");
            ImGui::MenuItem("Import...");
            ImGui::Separator();
            if (ImGui::MenuItem("Quit", "Alt+F4")) user_actions.fire("quit");

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            ImGui::MenuItem("Undo", user_actions.combo_str("undo"));
            ImGui::MenuItem("Redo", user_actions.combo_str("redo"));
            ImGui::Separator();
            ImGui::MenuItem("Select All", user_actions.combo_str("select_all"));
            ImGui::MenuItem("Select Channel", user_actions.combo_str("select_channel"));
            ImGui::Separator();
            ImGui::MenuItem("Copy Pattern", user_actions.combo_str("copy"));
            ImGui::MenuItem("Paste Pattern", user_actions.combo_str("paste"));
            ImGui::MenuItem("Paste Pattern Numbers", user_actions.combo_str("paste_pattern_numbers"));
            ImGui::Separator();
            ImGui::MenuItem("Insert Bar", user_actions.combo_str("insert_bar"));
            ImGui::MenuItem("Delete Bar", user_actions.combo_str("delete_bar"));
            ImGui::MenuItem("Delete Channel", user_actions.combo_str("delete_channel"));
            ImGui::MenuItem("Duplicate Reused Patterns", user_actions.combo_str("duplicate_patterns"));
            ImGui::Separator();
            ImGui::MenuItem("New Pattern", user_actions.combo_str("new_pattern"));
            ImGui::MenuItem("Move Notes Up", user_actions.combo_str("move_notes_up"));
            ImGui::MenuItem("Move Notes Down", user_actions.combo_str("move_notes_down"));
            
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Preferences"))
        {
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help"))
        {
            ImGui::MenuItem("About...");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Dev"))
        {
            if (ImGui::MenuItem("Toggle Dear ImGUI Demo", "F1")) {
                show_demo_window = !show_demo_window;
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    ///////////////////
    // SONG SETTINGS //
    ///////////////////

    ImGui::Begin("Song Settings");

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
    ImGui::Text("Tempo (bpm)");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputFloat("bpm##song_tempo", &song.tempo, 0.0f, 0.0f, "%.0f");
    if (song.tempo < 0) song.tempo = 0;

    ImGui::End();

    //////////////////////
    // CHANNEL SETTINGS //
    //////////////////////

    ImGui::Begin("Channel Settings", nullptr);

    Channel* cur_channel = song.channels[song.selected_channel];

    // channel name
    ImGui::PushItemWidth(-1.0f);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Name");
    ImGui::SameLine();
    ImGui::InputText("##channel_name", cur_channel->name, 64);

    // volume slider
    {
        float volume = cur_channel->vol_mod.volume * 100.0f;
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Volume");
        ImGui::SameLine();
        ImGui::SliderFloat("##channel_volume", &volume, 0, 100, "%.0f");
        cur_channel->vol_mod.volume = volume / 100.0f;
    }

    // panning slider
    {
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Panning");
        ImGui::SameLine();
        ImGui::SliderFloat("##channel_panning", &cur_channel->vol_mod.panning, -1, 1, "%.2f");
    }

    // fx mixer bus combobox
    ImGui::AlignTextToFramePadding();
    ImGui::Text("FX Bus");
    ImGui::SameLine();
    if (ImGui::BeginCombo("##channel_bus", bus_names[bus_index]))
    {
        for (int i = 0; i < 4; i++) {
            if (ImGui::Selectable(bus_names[i], i == bus_index)) bus_index = i;

            if (i == bus_index) {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }

    ImGui::PopItemWidth();
    ImGui::NewLine();

    // loaded instrument
    ImGui::Text("Instrument: [No Instrument]");
    if (ImGui::Button("Load...", ImVec2(ImGui::GetWindowSize().x / -2.0f, 0.0f)))
    {
        ImGui::OpenPopup("inst_load");
    }
    ImGui::SameLine();

    if (ImGui::Button("Edit...", ImVec2(-1.0f, 0.0f)))
    {
        ImGui::OpenPopup("inst_load");
    }

    if (ImGui::BeginPopup("inst_load"))
    {
        ImGui::Text("test");
        ImGui::EndPopup();
    }

    // effects
    ImGui::NewLine();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Effects");
    ImGui::SameLine();
    ImGui::Button("+##Add");

    ImGui::End();

    //////////////////
    // TRACK EDITOR //
    //////////////////

    {
        ImGui::Begin("Track Editor");

        // cell size including margin
        static const Vec2 CELL_SIZE = Vec2(26, 26);
        // empty space inbetween cells
        static const int CELL_MARGIN = 1;

        static int num_channels = song.channels.size();
        static int num_bars = song.length();

        ImGui::BeginChild(ImGui::GetID((void*)1209378), Vec2(-1, -1), false, ImGuiWindowFlags_HorizontalScrollbar);
        
        Vec2 canvas_size = ImGui::GetContentRegionAvail();
        Vec2 canvas_p0 = ImGui::GetCursorScreenPos();
        Vec2 canvas_p1 = canvas_p0 + canvas_size;
        Vec2 viewport_scroll = (Vec2)ImGui::GetWindowPos() - canvas_p0;
        Vec2 mouse_pos = Vec2(io.MousePos) - canvas_p0;
        Vec2 content_size = Vec2(num_bars, num_channels) * CELL_SIZE;

        int mouse_row = -1;
        int mouse_col = -1;

        ImGui::InvisibleButton("track_editor_mouse_target", content_size, ImGuiButtonFlags_MouseButtonLeft);
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

        for (int ch = col_start; ch < min(col_end, num_channels); ch++) {
            for (int bar = bar_start; bar < min(bar_end, num_bars); bar++) {
                Vec2 rect_pos = Vec2(canvas_p0.x + bar * CELL_SIZE.x + CELL_MARGIN, canvas_p0.y + CELL_SIZE.y * ch + CELL_MARGIN);
                int pattern_num = song.channels[ch]->sequence[bar];
                bool is_selected = song.selected_bar == bar && song.selected_channel == ch;

                // draw cell background
                if (pattern_num > 0 || is_selected)
                    draw_list->AddRectFilled(
                        rect_pos,
                        Vec2(rect_pos.x + CELL_SIZE.x - CELL_MARGIN * 2, rect_pos.y + CELL_SIZE.y - CELL_MARGIN * 2),
                        is_selected ? Colors::channel[ch % Colors::channel_num][1] : IM_RGB32(50, 50, 50)
                    );
                
                sprintf(str_buf, "%i", pattern_num); // convert pattern_num to string (too lazy to figure out how to do it the C++ way)
                
                // draw pattern number
                draw_list->AddText(
                    rect_pos + (CELL_SIZE - Vec2(CELL_MARGIN, CELL_MARGIN) * 2.0f - ImGui::CalcTextSize(str_buf)) / 2.0f,
                    is_selected ? IM_COL32_BLACK : Colors::channel[ch % Colors::channel_num][pattern_num > 0],
                    str_buf
                );

                // draw mouse hover
                if (ch == mouse_row && bar == mouse_col) {
                    Vec2 rect_pos = Vec2(canvas_p0.x + bar * CELL_SIZE.x, canvas_p0.y + CELL_SIZE.y * ch);
                    draw_list->AddRect(rect_pos, rect_pos + CELL_SIZE, IM_COL32_WHITE, 0.0f, 0, 1.0f);
                }
            }
        }

        // draw playhead
        double song_pos = song.is_playing ? (song.position / song.beats_per_bar) : (song.bar_position);
        Vec2 playhead_pos = canvas_p0 + Vec2(song_pos * CELL_SIZE.x, 0);
        draw_list->AddRectFilled(playhead_pos, playhead_pos + Vec2(1.0f, canvas_size.y), IM_COL32_WHITE);

        // set scrollable area
        ImGui::SetCursorPos(content_size);

        ImGui::EndChild();
        ImGui::End();
    }


    ////////////////////
    // PATTERN EDITOR //
    ////////////////////
    ImGui::Begin("Pattern Editor"); {
        // cell size including margin
        static const Vec2 CELL_SIZE = Vec2(50, 16);
        // empty space inbetween cells
        static const int CELL_MARGIN = 1;

        static constexpr float PIANO_KEY_WIDTH = 30;

        Vec2 canvas_size = ImGui::GetContentRegionAvail();
        Vec2 offset = Vec2(canvas_size.x - (CELL_SIZE.x * song.beats_per_bar + PIANO_KEY_WIDTH), 0) / 2.0f;

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
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::BeginCombo("##pattern_editor_step", step_names[selected_step]))
        {
            for (int i = 0; i < 5; i++) {
                if (ImGui::Selectable(step_names[i], i == selected_step)) {
                    selected_step = i;
                    song.editor_quantization = step_values[i];
                }

                if (i == selected_step) {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }

        ImGui::NewLine();

        canvas_size = ImGui::GetContentRegionAvail();

        float min_step = song.editor_quantization;

        static int scroll = 60;
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
        
        // this variable are used to determine whether the user simply clicked on a note
        // to delete it
        static Vec2 mouse_screen_start;
        static bool did_mouse_move;
        static bool play_key = false;
        static int prev_mouse_cy = 0;

        static bool is_adding_note;
        enum DragMode {
            FromLeft,
            FromRight,
            Any
        } static note_drag_mode;

        // center viewport
        ImGui::SetCursorPos(Vec2(ImGui::GetCursorPos()) + offset + Vec2(0, -CELL_SIZE.y));
        
        Vec2 canvas_p0 = ImGui::GetCursorScreenPos();
        Vec2 canvas_p1 = canvas_p0 + canvas_size;
        Vec2 draw_origin = canvas_p0;

        // define interactable area
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        Vec2 button_size = Vec2(CELL_SIZE.x * song.beats_per_bar + PIANO_KEY_WIDTH, canvas_size.y + style.WindowPadding.y);
        if (button_size.x <= 0.0f) button_size.x = 1.0f;
        if (button_size.y <= 0.0f) button_size.y = 1.0f;

        ImGui::InvisibleButton("pattern_editor_click_area",
            button_size,
            ImGuiButtonFlags_MouseButtonLeft
        );

        // get data for the currently selected channel
        static Channel* prev_channel = nullptr;
        Channel* selected_channel = song.channels[song.selected_channel];
        int pattern_id = selected_channel->sequence[song.selected_bar] - 1;
        Pattern* selected_pattern = nullptr;
        if (pattern_id >= 0) {
            selected_pattern = selected_channel->patterns[selected_channel->sequence[song.selected_bar] - 1];
        }

        //Vec2 viewport_scroll = (Vec2)ImGui::GetWindowPos() - canvas_p0;
        //Vec2 content_size = Vec2(num_bars, num_channels) * CELL_SIZE;
        Vec2 mouse_pos = Vec2(io.MousePos) - canvas_p0;
        float mouse_px = -1.0f; // cell position of mouse
        float mouse_cx = -1.0f; // mouse_px is in the center of the mouse cursor note
        int mouse_cy = mouse_cy = (int)mouse_pos.y / CELL_SIZE.y;
        
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
                if (selected_pattern != nullptr && ImGui::IsItemHovered()) {
                    mouse_start = mouse_px;

                    if (note_hovered) {
                        is_adding_note = false;
                        selected_note = note_hovered;

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
                        is_adding_note = true;
                        note_drag_mode = DragMode::Any;
                        selected_note = &selected_pattern->add_note(mouse_cx, scroll - mouse_cy, cursor_note_length);
                        note_anchor = mouse_cx;
                        note_start_length = cursor_note_length;
                    }

                    note_pattern = selected_pattern;
                }
            }

            // if in piano area, play a note
            else {
                // stop already currently playing note
                if (play_key) {
                    // turn off old note
                    selected_channel->synth_mod.event(audiomod::NoteEvent {
                        audiomod::NoteEventKind::NoteOff,
                        {
                            scroll - prev_mouse_cy
                        }
                    });
                }

                play_key = true;
                int key = scroll - mouse_cy;

                selected_channel->synth_mod.event(audiomod::NoteEvent {
                    audiomod::NoteEventKind::NoteOn,
                    {
                        key,
                        song.get_key_frequency(key),
                        0.2f
                    }
                });
            }
        }

        if (ImGui::IsItemActive()) {
            // detect if mouse had moved while down
            if (!did_mouse_move) {
                if ((mouse_pos - mouse_screen_start).magn_sq() > 5*5) {
                    did_mouse_move = true;
                }
            
            // piano key glissando
            } else if (play_key) {
                if (prev_mouse_cy != mouse_cy) {
                    // turn off old note
                    selected_channel->synth_mod.event(audiomod::NoteEvent {
                        audiomod::NoteEventKind::NoteOff,
                        {
                            scroll - prev_mouse_cy
                        }
                    });

                    // turn on new note
                    selected_channel->synth_mod.event(audiomod::NoteEvent {
                        audiomod::NoteEventKind::NoteOn,
                        {
                            scroll - mouse_cy,
                            song.get_key_frequency(scroll - mouse_cy),
                            0.2f
                        }
                    });
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
                prev_channel->synth_mod.event(audiomod::NoteEvent {
                    audiomod::NoteEventKind::NoteOff,
                    {
                        scroll - prev_mouse_cy
                    }
                });

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
                            note_pattern->notes.erase(it);
                            break;
                        }
                    }
                } else {
                    desired_cursor_len = selected_note->length;
                }

                note_pattern = nullptr;
                selected_note = nullptr;
            }

            // if currently playing a note
            if (play_key) {
                // turn off old note
                selected_channel->synth_mod.event(audiomod::NoteEvent {
                    audiomod::NoteEventKind::NoteOff,
                    {
                        scroll - prev_mouse_cy
                    }
                });

                play_key = false;
            }
        }

        // draw cells
        for (int row = 0; row < (int)canvas_size.y / CELL_SIZE.y + 2; row++) {
            int key = scroll - row;

            // draw piano key
            Vec2 piano_rect_pos = draw_origin + CELL_SIZE * Vec2(0, row);

            draw_list->AddRectFilled(
                piano_rect_pos + Vec2(CELL_MARGIN, CELL_MARGIN),
                piano_rect_pos + Vec2(PIANO_KEY_WIDTH - CELL_MARGIN * 2, CELL_SIZE.y - CELL_MARGIN * 2),
                key % 12 == 0 ? IM_RGB32(95, 23, 23) : ACCIDENTAL[key % 12] ? IM_RGB32(38, 38, 38) : IM_RGB32(20, 20, 20)
            );

            // get key name
            strcpy(key_name, KEY_NAMES[key % 12]);

            // if key is C, then add the octave number
            if (key % 12 == 0) sprintf(key_name + 1, "%i", key / 12);

            // draw key name
            Vec2 text_size = ImGui::CalcTextSize(KEY_NAMES[key % 12]);
            draw_list->AddText(
                piano_rect_pos + Vec2(5, (CELL_SIZE.y - text_size.y) / 2.0f),
                IM_COL32_WHITE,
                key_name
            );

            ImU32 row_color =
                key % 12 == 0 ? IM_RGB32(191, 46, 46) : // highlight each octave
                key % 12 == 7 ? IM_RGB32(74, 68, 68) : // highlight each fifth
                IM_RGB32(50, 50, 50); // default color

            // draw cells in this row
            for (int col = 0; col < 8; col++) {
                Vec2 cell_pos = draw_origin + CELL_SIZE * Vec2(col, row) + Vec2(PIANO_KEY_WIDTH, 0);
                Vec2 rect_pos = cell_pos + Vec2(CELL_MARGIN, CELL_MARGIN);

                draw_list->AddRectFilled(rect_pos, rect_pos + CELL_SIZE - Vec2(CELL_MARGIN, CELL_MARGIN) * 2.0f, row_color);
            }
        }

        if (selected_pattern != nullptr) {
            // draw pattern notes
            for (Note& note : selected_pattern->notes) {
                Vec2 cell_pos = draw_origin + CELL_SIZE * Vec2(note.time, scroll - note.key) + Vec2(PIANO_KEY_WIDTH, 0);
                Vec2 rect_pos = cell_pos + Vec2(CELL_MARGIN, 0);

                draw_list->AddRectFilled(rect_pos, rect_pos + CELL_SIZE * Vec2(note.length, 1.0f) - Vec2(CELL_MARGIN, 0) * 2.0f, Colors::channel[song.selected_channel][1]);
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
                    IM_COL32_WHITE
                );
            } else {
                Vec2 rect_pos = Vec2(draw_origin.x, draw_origin.y + CELL_SIZE.y * mouse_cy);
                
                draw_list->AddRect(
                    rect_pos, rect_pos + Vec2(PIANO_KEY_WIDTH, CELL_SIZE.y),
                    IM_COL32_WHITE
                );
            }
        }

        // draw playhead
        if (song.is_playing && selected_channel->sequence[song.bar_position] - 1 == pattern_id) {
            Vec2 playhead_pos = draw_origin + Vec2(PIANO_KEY_WIDTH + fmodf(song.position, song.beats_per_bar) * CELL_SIZE.x, 0.0f);
            draw_list->AddRectFilled(playhead_pos, playhead_pos + Vec2(1.0f, canvas_size.y + style.WindowPadding.y * 2.0f), IM_COL32_WHITE);
        }

        prev_mouse_cy = mouse_cy;

        ImGui::End();
    }

    // Info panel //
    ImGui::Begin("Info");
    ImGui::Text("framerate: %.2f", io.Framerate);
    ImGui::End();
}