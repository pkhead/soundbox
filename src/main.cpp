#include "../imgui/imgui.h"
#include "../imgui/backends/imgui_impl_glfw.h"
#include "../imgui/backends/imgui_impl_opengl3.h"
#include <stdio.h>
#include <GLFW/glfw3.h>

static void glfw_error_callback(int error, const char *description)
{
    fprintf(stderr, "GLFW error %d: %s\n", error, description);
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
    // glfwSwapInterval(1); // enable vsync

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

    char song_name[64] = "Untitled";
    char ch_name[64] = "Channel 1";
    float volume = 50;
    float panning = 0;
    int bus_index = 0;

    static const char* bus_names[] = {
        "0 - master",
        "1 - bus 1",
        "2 - bus 2",
        "3 - drums",
    };

    bool show_demo_window = false;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);

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
                if (ImGui::MenuItem("Toggle Dear ImGUI Demo")) {
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

        ImGui::Render();

        glViewport(0, 0, display_w, display_h);
        glClearColor(0.5, 0.7, 0.4, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }
}