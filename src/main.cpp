#include <stdio.h>
#include <string>
#include <vector>
#include "song.h"

#include "../imgui/imgui.h"
#include "../imgui/backends/imgui_impl_glfw.h"
#include "../imgui/backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#ifdef _WIN32
// Windows
#include <windows.h>

#define sleep(time) Sleep((time) * 1000)

#else
// Unix
#include <unistd.h>

#define sleep(time) usleep((time) * 1000000)

#endif

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

static bool show_demo_window = false;

static void glfw_error_callback(int error, const char *description)
{
    fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

static std::vector<int> key_press_queue;
static ImGuiIO* current_imgui_io = nullptr;

static void compute_imgui(ImGuiIO& io, Song& song) {
    ImGuiStyle& style = ImGui::GetStyle();

    static char song_name[64] = "Untitled";
    static float volume = 50;
    static float panning = 0;
    static int bus_index = 0;

    static const char* bus_names[] = {
        "0 - master",
        "1 - bus 1",
        "2 - bus 2",
        "3 - drums",
    };

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_AutoHideTabBar);
    if (show_demo_window) ImGui::ShowDemoWindow();

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            ImGui::MenuItem("New", "Ctrl+N");
            ImGui::MenuItem("Save", "Ctrl+S");
            ImGui::MenuItem("Save As...", "Ctrl+Shift+S");
            ImGui::Separator();
            ImGui::MenuItem("Export...");
            ImGui::MenuItem("Import...");
            ImGui::Separator();
            ImGui::MenuItem("Quit", "Ctrl+Q");

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            ImGui::MenuItem("Undo", "Ctrl+Z");
            ImGui::MenuItem("Redo", "Ctrl+Shift+Z"); // use CTRL+Y on windows
            
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
    ImGui::InputText("##song_name", song_name, 64);

    // play/prev/next
    ImGui::Button("Play", ImVec2(-1.0f, 0.0f));
    ImGui::Button("Prev", ImVec2(ImGui::GetWindowSize().x / -2.0f, 0.0f));
    ImGui::SameLine();
    ImGui::Button("Next", ImVec2(-1.0f, 0.0f));
    
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

    // channel name
    ImGui::PushItemWidth(-1.0f);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Name");
    ImGui::SameLine();
    ImGui::InputText("##channel_name", song.channels[song.selected_channel]->name, 64);

    // volume slider
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Volume");
    ImGui::SameLine();
    ImGui::SliderFloat("##channel_volume", &volume, 0, 100, "%.0f");

    // panning slider
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Panning");
    ImGui::SameLine();
    ImGui::SliderFloat("##channel_panning", &panning, -1, 1, "%.2f");

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

        // set scrollable area
        ImGui::SetCursorPos(content_size);

        ImGui::EndChild();
        ImGui::End();
    }


    ////////////////////
    // PATTERN EDITOR //
    ////////////////////
    {
        ImGui::Begin("Pattern Editor");

        static constexpr int NUM_DIVISIONS = 8;
        static constexpr float PIANO_KEY_WIDTH = 30;
        static int scroll = 60;
        static const char* KEY_NAMES[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
        static const bool ACCIDENTAL[12] = {false, true, false, true, false, false, true, false, true, false, true, false};
        static char key_name[8];
        static float cursor_note_length = 1.0f;
        static float min_step = 0.25f;

        // cell size including margin
        static const Vec2 CELL_SIZE = Vec2(50, 16);
        // empty space inbetween cells
        static const int CELL_MARGIN = 1;
        
        Vec2 canvas_size = ImGui::GetContentRegionAvail();
        ImGui::SetCursorPos(Vec2(canvas_size.x - (CELL_SIZE.x * NUM_DIVISIONS + PIANO_KEY_WIDTH), 0) / 2.0f + Vec2(style.WindowPadding.x, 0));
        Vec2 canvas_p0 = ImGui::GetCursorScreenPos();
        Vec2 canvas_p1 = canvas_p0 + canvas_size;
        Vec2 draw_origin = canvas_p0;

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImGui::InvisibleButton("pattern_editor_click_area",
            Vec2(CELL_SIZE.x * NUM_DIVISIONS + PIANO_KEY_WIDTH, canvas_size.y + style.WindowPadding.y),
            ImGuiButtonFlags_MouseButtonLeft);

        //Vec2 viewport_scroll = (Vec2)ImGui::GetWindowPos() - canvas_p0;
        //Vec2 content_size = Vec2(num_bars, num_channels) * CELL_SIZE;
        Vec2 mouse_pos = Vec2(io.MousePos) - canvas_p0;
        float mouse_px = -1.0f; // cell position of mouse
        float mouse_cx = -1.0f; // mouse_px is in the center of the mouse cursor note
        int mouse_cy = -1;
        
        // calculate mouse grid position
        if (mouse_pos.x > PIANO_KEY_WIDTH) {
            mouse_px = int(((mouse_pos.x - PIANO_KEY_WIDTH) / CELL_SIZE.x) / min_step + 0.5) * min_step;
            mouse_cx = int(((mouse_pos.x - PIANO_KEY_WIDTH) / CELL_SIZE.x - cursor_note_length / 2.0f) / min_step + 0.5) * min_step;
            if (mouse_cx < 0) mouse_cx = 0;
            if (mouse_cx + cursor_note_length > NUM_DIVISIONS) mouse_cx = (float)NUM_DIVISIONS - cursor_note_length;
        }

        mouse_cy = (int)mouse_pos.y / CELL_SIZE.y;

        // get data for the currently selected channel
        Channel* selected_channel = song.channels[song.selected_channel];
        int pattern_id = selected_channel->sequence[song.selected_bar] - 1;
        Pattern* selected_pattern = nullptr;
        if (pattern_id >= 0) {
            selected_pattern = selected_channel->patterns[selected_channel->sequence[song.selected_bar] - 1];
        }

        static Note* note_hovered = nullptr;
        static Note* selected_note = nullptr;
        static Pattern* note_pattern = nullptr;
        static float note_anchor;

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

        if (mouse_cx >= 0.0f && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            // add note on click
            if (selected_pattern != nullptr && ImGui::IsItemHovered()) {
                if (note_hovered) {
                    selected_note = note_hovered;
                    note_anchor = selected_note->time;
                } else {
                    selected_note = &selected_pattern->add_note(mouse_cx, scroll - mouse_cy, 1.0f);
                    note_anchor = mouse_cx;
                }

                note_pattern = selected_pattern;
            }
        }

        if (selected_note != nullptr) {
            float dx = mouse_cx - note_anchor + cursor_note_length;

            if (dx >= 0) {
                if (dx < min_step) dx = min_step;

                selected_note->time = note_anchor;
                selected_note->length = dx;

                if (selected_note->time + selected_note->length > NUM_DIVISIONS) {
                    selected_note->length = (float)NUM_DIVISIONS - selected_note->time;
                }
                
            } else {
                if (dx > -min_step) dx = -min_step;

                selected_note->time = note_anchor + dx;
                selected_note->length = -dx;

                if (selected_note->time < 0) {
                    selected_note->time = 0;
                    selected_note->length = note_anchor;
                }
            }
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            selected_note = nullptr;
        }

        // draw cells
        for (int row = -1; row < (int)canvas_size.y / CELL_SIZE.y + 1; row++) {
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

        ImGui::End();
    }

    // Info panel //
    ImGui::Begin("Info");
    ImGui::Text("framerate: %.2f", io.Framerate);
    ImGui::End();
}

int main()
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // OpenGL 3.3 + GLSL 330
    const char *glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // create window w/ graphics context
    GLFWwindow *window = glfwCreateWindow(1280, 720, "soundbox", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0); // disable vsync

    // setup dear imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsClassic();

    Song song(4, 100, 8);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            song.channels[i]->sequence[j] = 1;
        }
    }

    int pattern_input = 0;
    
    // if one of these variables changes, then clear pattern_input
    int last_selected_bar = song.selected_bar;
    int last_selected_ch = song.selected_channel;

    static const double FRAME_LENGTH = 1.0 / 120.0;

    double next_time = glfwGetTime();

    while (!glfwWindowShouldClose(window))
    {
        next_time = glfwGetTime() + FRAME_LENGTH;

        key_press_queue.clear();
        glfwPollEvents();

        // if selected pattern changed
        if (last_selected_bar != song.selected_bar || last_selected_ch != song.selected_channel) {
            last_selected_bar = song.selected_bar;
            last_selected_ch = song.selected_channel;
            pattern_input = 0;
        }

        // key input
        if (!io.WantTextInput) {
            // track editor controls
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
                song.selected_bar++;
                song.selected_bar %= song.length();
            }

            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
                song.selected_bar--;
                if (song.selected_bar < 0) song.selected_bar = song.length() - 1;
            }

            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
                song.selected_channel++;
                song.selected_channel %= song.channels.size();
            }

            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
                song.selected_channel--;
                if (song.selected_channel < 0) song.selected_channel = song.channels.size() - 1;
            }

            // track editor pattern entering
            for (int k = 0; k < 10; k++) {
                if (ImGui::IsKeyPressed((ImGuiKey)((int)ImGuiKey_0 + k))) {
                    pattern_input = (pattern_input * 10) + k;
                    if (pattern_input > song.max_patterns()) pattern_input = k;

                    song.channels[song.selected_channel]->sequence[song.selected_bar] = pattern_input;
                }
            }
        }

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        
        compute_imgui(io, song);

        ImGui::Render();

        glViewport(0, 0, display_w, display_h);
        glClearColor(0.5, 0.7, 0.4, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);

        double cur_time = glfwGetTime();

        if (next_time <= cur_time) {
            next_time = cur_time;
        }
        
        sleep(next_time - cur_time);
    }
}