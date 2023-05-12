#include <stdio.h>
#include "song.h"

#include "../imgui/imgui.h"
#include "../imgui/backends/imgui_impl_glfw.h"
#include "../imgui/backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

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

    inline Vec2 operator+(const Vec2& other) {
        return Vec2(x + other.x, y + other.y);
    }
    inline Vec2 operator-(const Vec2& other) {
        return Vec2(x - other.x, y - other.y);
    }
    inline Vec2 operator*(const Vec2& other) {
        return Vec2(x * other.x, y * other.y);
    }
    inline operator ImVec2() const { return ImVec2(x, y); }
};

static bool show_demo_window = false;

static void glfw_error_callback(int error, const char *description)
{
    fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

static void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_F1) {
            show_demo_window = !show_demo_window;
        }
    }
}

static void compute_imgui(ImGuiIO& io, Song& song) {
    static char song_name[64] = "Untitled";
    static char ch_name[64] = "Channel 1";
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
    ImGui::SameLine();

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
    ImGui::InputText("##channel_name", ch_name, 64);

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

    ImGui::Begin("Track Editor");

    // cell size including margin
    static const Vec2 CELL_SIZE = Vec2(24, 24);
    // empty space inbetween cells
    static const int CELL_MARGIN = 2;

    static int num_channels = song.channels.size();
    static int num_bars = song.length();

    ImGuiStyle& style = ImGui::GetStyle();

    {
        ImGui::BeginChild(ImGui::GetID((void*)1209378), Vec2(-1, -1), false, ImGuiWindowFlags_HorizontalScrollbar);
        
        Vec2 canvas_size = ImGui::GetContentRegionAvail();
        Vec2 canvas_p0 = ImGui::GetCursorScreenPos();
        Vec2 canvas_p1 = canvas_p0 + canvas_size;
        Vec2 viewport_scroll = (Vec2)ImGui::GetWindowPos() - canvas_p0;
        
        // use canvas for rendering
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        // visible bounds of viewport
        int row_start = (int)viewport_scroll.x / CELL_SIZE.x;
        int row_end = row_start + (int)canvas_size.x / CELL_SIZE.x + 2;
        int col_start = (int)viewport_scroll.y / CELL_SIZE.y;
        int col_end = col_start + (int)canvas_size.y / CELL_SIZE.y + 2;

        for (int ch = col_start; ch < min(col_end, num_channels); ch++) {
            for (int row = row_start; row < min(row_end, num_bars); row++) {
                Vec2 rect_pos = Vec2(canvas_p0.x + row * CELL_SIZE.x + CELL_MARGIN, canvas_p0.y + CELL_SIZE.y * ch + CELL_MARGIN);
                draw_list->AddRectFilled(rect_pos, Vec2(rect_pos.x + CELL_SIZE.x - CELL_MARGIN * 2, rect_pos.y + CELL_SIZE.y - CELL_MARGIN * 2), IM_COL32(50, 50, 50, 255));
                draw_list->AddText(rect_pos + Vec2(7, 4), IM_COL32(255, 255, 255, 255), "0");
            }
        }

        ImGui::SetCursorPos(Vec2(num_bars, num_channels) * CELL_SIZE);

        ImGui::EndChild();
    }

    ImGui::End();

    // Info panel //
    ImGui::Begin("Info");
    ImGui::Text("ms render: %.2f", 1000.0f / io.Framerate);
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
    // glfwSwapInterval(0); // enable vsync

    // setup dear imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    glfwSetKeyCallback(window, glfw_key_callback);

    Song song(4, 32, 8);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        
        compute_imgui(io, song);

        ImGui::Render();

        glViewport(0, 0, display_w, display_h);
        glClearColor(0.5, 0.7, 0.4, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }
}