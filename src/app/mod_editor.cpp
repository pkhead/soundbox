#include <cfloat>
#include <imgui.h>
#include <algorithm>
#include <numutil.hpp>
#include <imgui_internal.h>
#include <log.hpp>
#include <audio_engine/audio_engine.hpp>
#include <module_hosts/internal/host.hpp>
#include "mod_editor.hpp"
#include "shortcuts.hpp"

using namespace sbox;

ModuleEditor::ModuleEditor(Song &song, ShortcutContext &shortcuts) :
    song(song),
    shortcuts(shortcuts)
{
    module_list_by_author();
}

static bool mod_info_filter(const modules::ModuleInfo &info)
{
    const auto &hidden_mod_classes = hosts::internal::InternalModuleHost::hidden_mod_classes;
    if (std::find(hidden_mod_classes.begin(), hidden_mod_classes.end(), info.class_name) != hidden_mod_classes.end())
        return false;

    return true;
}

void ModuleEditor::module_list_by_author()
{
    _module_list.clear();
    std::unordered_map<std::string, unsigned int> author_category_map;

    for (auto &modclass : song.audio_engine().available_module_classes())
    {
        ModuleCategory *category = nullptr;

        const auto &map_it = author_category_map.find(modclass.author);
        if (map_it == author_category_map.end())
        {
            author_category_map[modclass.author] = _module_list.size();
            _module_list.push_back(ModuleCategory {
                .name = modclass.author
            });
            category = &_module_list.back();
        }
        else
        {
            category = &_module_list[map_it->second];
        }

        if (mod_info_filter(modclass))
            category->modules.push_back(modclass);
    }
}

void ModuleEditor::module_list_by_host()
{
    assert(false);
}

unsigned int get_required_input_connections(ModuleRack &rack, unsigned int index)
{
    assert(index < rack.size());
    modx::ModuleRc &mod = rack.at(index);

    const modx::ModuleRc &in = index == 0 ? rack.input() : rack.at(index-1);
    assert(in->valid());

    return in->audio_output_channel_count(0);
}

unsigned int get_required_output_connections(ModuleRack &rack, unsigned int index)
{
    assert(index < rack.size());
    modx::ModuleRc &mod = rack.at(index);

    const modx::ModuleRc &out = index == rack.size() - 1 ? rack.output() : rack.at(index+1);
    assert(out->valid());

    return out->audio_input_channel_count(0);
}

std::string channel_count_name(unsigned int count)
{
    assert(count > 0);
    if (count == 1)
    {
        return "mono";
    }
    else if (count == 2)
    {
        return "stereo";
    }
    else
    {
        return std::to_string(count) + "-channel";
    }
}

// for a module that failed to connect with the rest of the rack,
// display the reason of failure.
// i.e. the fact that the module is mono
// trying my best to make it sound like natural english, lol
void display_error_reason(ModuleRack &rack, unsigned int index)
{
    auto &mod = rack.at(index);
    unsigned int this_in = mod->audio_input_channel_count(0);
    unsigned int this_out = mod->audio_output_channel_count(0);
    unsigned other_in = get_required_input_connections(rack, index);
    unsigned other_out = get_required_output_connections(rack, index);
    
    if (this_in != other_in)
    {
        ImGui::TextWrapped("Input expects %s audio, but is given %s audio.", channel_count_name(this_in).c_str(), channel_count_name(other_out).c_str());
    }

    if (this_out != other_out)
    {
        ImGui::TextWrapped("Module outputs %s audio, but the next module requires %s audio.", channel_count_name(this_out).c_str(), channel_count_name(other_out).c_str());
    }
}

void ModuleEditor::draw()
{
    float mod_ui_height = ImGui::GetFontSize() * 17.0f;
    modules::ModuleID hovered_module_ui = 0;

    //ImGui::SetNextWindowSizeConstraints(ImVec2(0.0fmod_ui_width, 0.0f), ImVec2(mod_ui_width, FLT_MAX));

    if (ImGui::Begin("Module Editor", nullptr, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar))
    {
        assert(selected_channel_type == CHANNEL_TYPE_INSTRUMENT);
        InstrumentChannel &channel = song.get_channel(selected_channel);

        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::GetIO().MouseWheel != 0.0f)
        {
            ImGuiWindow *window = ImGui::GetCurrentWindow();

            // detect that it is not hovered over any scrollable child windows
            bool scroll_override = true;
            ImGuiWindow *child_window = ImGui::GetCurrentContext()->HoveredWindowUnderMovingWindow;
            while (child_window != window)
            {
                if (child_window->ScrollbarY || child_window->ScrollbarX)
                {
                    scroll_override = false;
                    break;
                }

                child_window = child_window->ParentWindow;
            }

            if (scroll_override)
            {
                float max_step = window->InnerRect.GetWidth() * 0.67f;
                float scroll_step = ImTrunc(ImMin(2 * window->CalcFontSize(), max_step));
                ImGui::SetScrollX(window, window->Scroll.x - ImGui::GetIO().MouseWheel * scroll_step);
            }
            //float sx = ImGui::GetScrollX();
            //ImGui::SetScrollX(sx - ImGui::GetIO().MouseWheel * 30.0f);
        }

        int module_to_delete = -1;
        int index_of_module_to_add = -1;
        std::string module_to_add;

        for (int i = -1; i < (int) channel.rack.size(); i++)
        {
            // iterator starts from one before the end of the array so that
            // it can display the button to insert a module at the start
            if (i >= 0)
            {
                modx::ModuleRc &mod = channel.rack.at(i);
                modx::ModuleBase *mod_data = modx::ModuleHost::get_module(mod->id());
                assert(mod_data != nullptr);

                ImGui::PushID(mod->id());

                ImGui::SameLine();

                // check that the module is connected properly...
                // will not be if the channel counts are mismatched
                bool connected = true;
                modules::ModuleID other_mod;
                unsigned int other_index;
                if (mod->audio_input_count() > 0)
                {
                    mod->engine().get_audio_input_connection(mod->id(), 0, other_mod, other_index);
                    if (other_mod == 0) connected = false;
                }

                if (mod->audio_output_count() > 0)
                {
                    mod->engine().get_audio_output_connection(mod->id(), 0, other_mod, other_index);
                    if (other_mod == 0) connected = false;
                }

                ImGuiChildFlags child_flags = ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_Border;
                if (connected) ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetColorU32(ImGuiCol_PopupBg, 0.4f));
                
                ImGui::BeginChild("module ui", ImVec2(0.0f, mod_ui_height), child_flags, ImGuiWindowFlags_MenuBar);
                if (connected) ImGui::PopStyleColor();

                if (ImGui::BeginMenuBar())
                {
                    ImVec2 start_cursor = ImGui::GetCursorPos();

                    ImGui::SetNextItemAllowOverlap();
                    ImVec2 drag_area_size = ImGui::GetContentRegionAvail();
                    drag_area_size.x = util::max(drag_area_size.x, 2.0f);

                    if (drag_area_size.x > 0.0f && drag_area_size.y > 0.0f)
                    {
                        ImGui::InvisibleButton("##DragArea", drag_area_size);

                        if (ImGui::IsItemHovered())
                        {
                            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                            hovered_module_ui = mod->id();
                        }

                        if (ImGui::IsItemActivated())
                        {
                            logger::log_debug("begin module drag");
                        }
                    }

                    ImGui::SetCursorPos(start_cursor);
                    ImGui::Text("%s", mod->name().c_str());

                    if (!connected)
                    {
                        ImGui::SameLine();
                        ImGui::TextDisabled("(!)");
                        if (ImGui::BeginItemTooltip())
                        {
                            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 20.0f);
                            ImGui::TextWrapped("The module could not connect properly!");
                            display_error_reason(channel.rack, i);
                            ImGui::PopTextWrapPos();
                            ImGui::EndTooltip();
                        }
                    }

                    ImGui::Separator();

                    if (mod_data->has_presets())
                    {
                        if (ImGui::BeginMenu("Presets"))
                        {
                            ImGui::MenuItem("Save Preset...");
                            if (ImGui::BeginMenu("Load Preset"))
                            {
                                for (int i = 0; i < 30; i++)
                                {
                                    ImGui::MenuItem("Preset");
                                }
                                ImGui::EndMenu();
                            }
                            ImGui::EndMenu();
                        }
                    }

                    float button_width = ImGui::GetFontSize();
                    ImGui::Dummy(ImVec2(button_width, ImGui::GetFrameHeight()));

                    ImGui::SameLine(ImGui::GetWindowWidth() - button_width - ImGui::GetStyle().ItemSpacing.x * 2.0f);
                    bool delete_module = ImGui::CloseButton(
                        ImGui::GetID("X"),
                        Vec2(ImGui::GetCursorScreenPos()) + Vec2(0.0f, (ImGui::GetFrameHeight() - ImGui::GetFontSize()) / 2.0f)
                    );

                    if (delete_module) module_to_delete = i;
                    //ImGui::Button("X", ImVec2(button_width, 0.0f));

                    //ImGui::SetCursorPos(start_cursor);

                    ImGui::EndMenuBar();
                }

                if (!connected) ImGui::BeginDisabled();
                mod_data->ui();
                if (!connected) ImGui::EndDisabled();

                ImGui::EndChild();
                ImGui::PopID();
            }

            // module insertion button
            if (i >= 0 || channel.rack.size() == 0)
            {
                ImGui::PushID(i);
                {
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_FrameBg));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetColorU32(ImGuiCol_FrameBgActive));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_FrameBgHovered));
                    if (ImGui::Button("+", ImVec2(ImGui::GetFontSize(), -FLT_MIN)))
                    {
                        ImGui::OpenPopup("Create Module");
                    }
                    ImGui::PopStyleColor(3);

                    if (ImGui::BeginPopup("Create Module"))
                    {
                        for (auto &category : _module_list)
                        {
                            if (ImGui::BeginMenu(category.name.c_str()))
                            {
                                for (auto &mod_info : category.modules)
                                {
                                    if (!mod_info.has_audio_input && i+1 > 0) continue;
                                    if (mod_info.has_audio_input && i+1 == 0) continue;

                                    if (ImGui::Selectable(mod_info.name.c_str()))
                                    {
                                        module_to_add = mod_info.class_name;
                                        index_of_module_to_add = i+1;
                                    }
                                }

                                ImGui::EndMenu();
                            }
                        }

                        ImGui::EndPopup();
                    }
                }
                
                ImGui::PopID();
            }
        }

        // perform rack-mutating actions after the loop ends
        if (index_of_module_to_add != -1)
            channel.rack.insert(modx::create_module(song.audio_engine(), module_to_add), index_of_module_to_add);

        if (module_to_delete != -1)
            channel.rack.remove(module_to_delete);

        ImGui::SameLine();
    } ImGui::End();
}