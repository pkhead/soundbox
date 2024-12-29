#include <cfloat>
#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <imgui_internal.h>
#include <imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <util.hpp>
#include <log.hpp>
#include <audio_engine/audio_engine.hpp>
#include <module_hosts/internal/host.hpp>
#include <module_hosts/internal/modules.hpp>
#include "mod_editor.hpp"
#include "app/song.hpp"
#include "audio_engine/module_data.hpp"
#include "module_hosts/modules.hpp"
#include "shortcuts.hpp"
#include "widgets.hpp"

using namespace sbox;

/////////////////
// Module list //
/////////////////
static bool mod_info_filter(const modules::ModuleInfo &info)
{
    const auto &hidden_mod_classes = hosts::internal::InternalModuleHost::hidden_mod_classes;
    if (std::find(hidden_mod_classes.begin(), hidden_mod_classes.end(), info.class_name) != hidden_mod_classes.end())
        return false;

    return true;
}

void ModuleList::module_list_by_author(const std::vector<modules::ModuleInfo> &list)
{
    std::unordered_map<std::string, unsigned int> author_category_map;

    for (auto &modclass : list)
    {
        ModuleCategory *category = nullptr;

        const auto &map_it = author_category_map.find(modclass.author);
        if (map_it == author_category_map.end())
        {
            author_category_map[modclass.author] = module_list.size();
            module_list.push_back(ModuleCategory(modclass.author));
            category = &module_list.back();
        }
        else
        {
            category = &module_list[map_it->second];
        }

        if (mod_info_filter(modclass))
            category->modules.push_back(modclass);
    }
}

void ModuleList::module_list_by_host(const std::vector<modules::ModuleInfo> &list)
{
    assert(false);
}

///////////////////
// Module editor //
///////////////////
ModuleEditor::ModuleEditor(SongEditor &editor, ModuleList &mod_list) :
    editor(editor),
    module_list(mod_list)
{}

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

inline static void left_aligned_label(const char *label)
{
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s", label);
};

static void channel_fader_controls(modx::ModuleRc &fader)
{
    // volume slider
    {
        const float min_value = -25.0f;
        const float max_value = 25.0f;
        float gain = fader->control_get_value<float>(hosts::internal::FaderModule::FADER_CONTROL_GAIN);

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
        bool changed = ImGui::SliderFloat("##channel_panning", &panning, -1, 1, "%.2f");
        if (ImGui::IsItemClicked(ImGuiMouseButton_Middle))
        {
            panning = 0.0f;
            changed = true;
        }

        if (changed) fader->control_set_value<float>(hosts::internal::FaderModule::FADER_CONTROL_PAN, panning);
    }
}

static bool fx_channel_combobox(Song &song, unsigned int *fx_channel_index, unsigned int ignore_channel = (unsigned int)-1)
{
    std::string cur_display_name;
    if (*fx_channel_index == (unsigned int)-1)
        cur_display_name = "(none)";
    else
        cur_display_name = util::format("%u - %s", *fx_channel_index, song.get_effect_channel(*fx_channel_index).name.c_str());
    
    bool changed = false;

    if (ImGui::BeginCombo("##channel_fx_target", cur_display_name.c_str()))
    {
        // list potential targets
        for (unsigned int target_i = 0; target_i < song.effect_channel_count(); target_i++)
        {
            if (target_i == ignore_channel) continue;

            auto& target_bus = song.get_effect_channel(target_i);

            // write target bus name
            std::string display_name = util::format("%u - %s", target_i, target_bus.name.c_str());

            bool is_selected = target_i == *fx_channel_index;
            if (ImGui::Selectable(display_name.c_str(), is_selected))
            {
                *fx_channel_index = target_i;
                changed = true;
            }

            if (is_selected) ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    return changed;
}

static void mod_presets(modx::ModuleBase *const mod_data, modx::ModuleRc &mod) {
    if (ImGui::BeginMenu("Presets"))
    {
        if (ImGui::MenuItem("Save Preset...")) {
            auto f = std::ofstream("test.bin");
            if (f.is_open()) {
                mod_data->save(f);
            }
        }
        
        if (ImGui::BeginMenu("Load Preset"))
        {
            if (ImGui::MenuItem("Button That Does Something")) {
                auto f = std::ifstream("test.bin");
                if (f.is_open()) {
                    mod_data->load(f);
                }
            }

            for (int i = 0; i < 30; i++)
            {
                ImGui::MenuItem("Preset");
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }
}

static void mod_ui(
    modx::ModuleBase *const mod_data, modx::ModuleRc &mod,
    Channel *const channel, int module_index,
    modules::ModuleID &hovered_module_ui, bool &do_delete_module
) {
    const float mod_ui_height = ImGui::GetFontSize() * 17.0f;
    do_delete_module = false;

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

    bool popup_modulators = false;

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

        if (ImGui::BeginMenu(mod->name().c_str())) {
            if (mod_data->has_presets())
                mod_presets(mod_data, mod);
                
            if (ImGui::MenuItem("Modulators"))
                popup_modulators = true;

            ImGui::EndMenu();
        }

        if (!connected)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("(!)");
            if (ImGui::BeginItemTooltip())
            {
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 20.0f);
                ImGui::TextWrapped("The module could not connect properly!");
                display_error_reason(channel->rack, module_index);
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
        }

        //ImGui::Separator();

        float button_width = ImGui::GetFontSize();
        ImGui::Dummy(ImVec2(button_width * 2.0f, ImGui::GetFrameHeight()));

        ImGui::SameLine(ImGui::GetWindowWidth() - button_width - ImGui::GetStyle().ItemSpacing.x * 2.0f);
        do_delete_module = ImGui::CloseButton(
            ImGui::GetID("X"),
            Vec2(ImGui::GetCursorScreenPos()) + Vec2(0.0f, (ImGui::GetFrameHeight() - ImGui::GetFontSize()) / 2.0f)
        );

        //ImGui::Button("X", ImVec2(button_width, 0.0f));

        //ImGui::SetCursorPos(start_cursor);

        ImGui::EndMenuBar();
    }

    if (popup_modulators) {
        ImGui::OpenPopup("Modulators");
    }

    if (ImGui::BeginPopup("Modulators")) {
        if (ImGui::BeginTabBar("Modulators", ImGuiTabBarFlags_Reorderable)) {
            constexpr const char *tab_names[3] = { "Tab 1", "Tab 2", "Tab 3 "};

            for (int i = 0; i < mod->modulator_count(); i++) {
                auto modsrc = mod->modulator_get_source(i);
                assert(modsrc != 0);
                auto type = mod->engine().get_modsrc_type(modsrc);

                const char *tab_name;
                switch (type) {
                    case modules::ModulatorSourceType::ENVELOPE:
                        tab_name = "Env";
                        break;

                    case modules::ModulatorSourceType::LFO:
                        tab_name = "LFO";
                        break;

                    case modules::ModulatorSourceType::UNKNOWN:
                        tab_name = "Unknown";
                        break;
                }

                ImGui::PushID(modsrc);
                if (ImGui::BeginTabItem(tab_name)) {
                    // target combobox
                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("Target");
                    ImGui::SameLine();
                    
                    std::vector<unsigned int> targets;
                    bool s = mod->modulator_get_targets(i, targets);
                    assert(s);
                    assert(targets.size() < 2);

                    std::string target_name = "(none)";
                    bool has_target = targets.size() == 1;
                    if (has_target) {
                        target_name = mod->control_name(targets[0]);
                    }

                    if (ImGui::BeginCombo("##Target", target_name.c_str())) {
                        for (unsigned int ctl_idx = 0; ctl_idx < mod->control_count(); ctl_idx++) {
                            std::string nm = mod->control_name(ctl_idx);

                            if (ImGui::Selectable(nm.c_str(), has_target && ctl_idx == targets[0])) {
                                if (has_target) mod->modulator_untarget(i, targets[0]);
                                mod->modulator_target(i, ctl_idx);
                            }
                        }

                        ImGui::EndCombo();
                    }

                    //mod->control_name(unsigned int index)

                    auto modsrc = mod->modulator_get_source(i);
                    assert(modsrc != 0);

                    // envelope parameters
                    modules::ModulatorSourceParams params;
                    s = mod->engine().get_modsrc_params(modsrc, params);
                    assert(s);

                    widgets::adsr_ui_struct adsr_data(params.attack, params.decay, params.sustain, params.release);
                    if (widgets::adsr_ui("Envelope", ImVec2(ImGui::GetFontSize() * 12.0f, 0.0f), &adsr_data)) {
                        params.attack = adsr_data.attack;
                        params.decay = adsr_data.decay;
                        params.sustain = adsr_data.sustain;
                        params.release = adsr_data.release;
                        mod->engine().set_modsrc_params(modsrc, params);
                    }

                    ImGui::Text("test");
                    ImGui::EndTabItem();
                }
                ImGui::PopID();
            }

            if (ImGui::TabItemButton("+")) {
                
                try {
                    unsigned int mod_index;
                    if (!mod->create_modulator(mod_index)) throw;

                    auto modsrc = mod->engine().create_modsrc(modules::ModulatorSourceType::ENVELOPE);
                    if (modsrc == 0) throw;

                    if (!mod->modulator_set_source(mod_index, modsrc)) throw;
                } catch(...) {
                    logger::log_error("could not create modulator");
                }

            }
            
            ImGui::EndTabBar();
        }
        ImGui::EndPopup();
    }

    if (!connected) ImGui::BeginDisabled();
    mod_data->ui();
    if (!connected) ImGui::EndDisabled();

    ImGui::EndChild();
}

void ModuleEditor::render_channel_settings(Channel &cur_channel)
{
    Song &song = editor.song;

    // left side: widget labels
    ImGui::BeginGroup();
    left_aligned_label("Name");
    left_aligned_label("Volume");
    left_aligned_label("Panning");
    if (channel_type == CHANNEL_TYPE_INSTRUMENT || selected_channel != 0) left_aligned_label("Output FX");
    ImGui::EndGroup();

    ImGui::SameLine();

    ImGui::BeginGroup();

    // channel name
    ImGui::PushItemWidth(-1.0f);
    ImGui::InputText("##channel_name", &cur_channel.name);

    auto &fader = cur_channel.output_fader;
    channel_fader_controls(fader);
    
    // fx channel combobox
    // don't render fx channel 0 because that's the master channel
    unsigned int out_fx_index = cur_channel.get_output_channel();

    if (channel_type == CHANNEL_TYPE_EFFECT)
    {
        if (selected_channel != 0)
        {
            if (fx_channel_combobox(song, &out_fx_index, selected_channel))
                song.route_effect(selected_channel, out_fx_index);
        }
    }
    else if (channel_type == CHANNEL_TYPE_INSTRUMENT)
    {
        if (fx_channel_combobox(song, &out_fx_index))
            song.route_instrument(selected_channel, out_fx_index);
    }
    else assert(false);

    ImGui::PopItemWidth();
    ImGui::EndGroup();
}

void ModuleEditor::draw(const char *window_title)
{
    modules::ModuleID hovered_module_ui = 0;

    //ImGui::SetNextWindowSizeConstraints(ImVec2(0.0fmod_ui_width, 0.0f), ImVec2(mod_ui_width, FLT_MAX));

    if (ImGui::Begin(window_title, nullptr))
    {
        // true if the first module in a module rack must be an instrument
        bool first_module_instrument = false;
        
        Channel *channel = nullptr;
        if (channel_type == CHANNEL_TYPE_EFFECT)
        {
            channel = &editor.song.get_effect_channel(selected_channel);
            first_module_instrument = false;
        }
        else if (channel_type == CHANNEL_TYPE_INSTRUMENT)
        {
            first_module_instrument = true;
            channel = &editor.song.get_channel(selected_channel);
        }
        else
        {
            throw std::runtime_error("ModuleEditor::channel_type is not CHANNEL_TYPE_EFFECT or CHANNEL_TYPE_INSTRUMENT");
        }

        // channel settings
        ImGui::BeginChild("Settings", Vec2(ImGui::GetFontSize() * 20.0f, ImGui::GetContentRegionAvail().y));
        {
            render_channel_settings(*channel);
        }
        ImGui::EndChild();

        // module settings
        ImGui::SameLine();
        ImGui::BeginChild("Modules", ImGui::GetContentRegionAvail(), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar);
        {
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

            for (int i = -1; i < (int) channel->rack.size(); i++)
            {
                // iterator starts from one before the end of the array so that
                // it can display the button to insert a module at the start
                if (i >= 0)
                {
                    modx::ModuleRc &mod = channel->rack.at(i);
                    modx::ModuleBase *mod_data = modx::ModuleHost::get_module(mod->id());
                    assert(mod_data != nullptr);

                    ImGui::PushID(mod->id());

                    ImGui::SameLine();

                    bool delete_module;
                    mod_ui(mod_data, mod, channel, i, hovered_module_ui, delete_module);
                    if (delete_module) module_to_delete = i;
                    
                    ImGui::PopID();
                }

                // module insertion button
                if (i >= 0 || channel->rack.size() == 0)
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
                            for (const auto &category : module_list)
                            {
                                if (ImGui::BeginMenu(category.name.c_str()))
                                {
                                    for (auto &mod_info : category.modules)
                                    {
                                        // don't show this module if it is required that the first module is a synthesizer
                                        // and... this is the first module, and this is not a synthesizer
                                        // also, omit modules that don't have audio inputs, except if it's the first module
                                        // and it is required that the first module is a synthesizer.
                                        if (first_module_instrument && i+1 <= 0)
                                        {
                                            //if (mod_info.midi_input != 0 && i+1 == 0) continue;
                                            if (mod_info.audio_input == 0) continue;
                                        }
                                        else
                                        {
                                            if (mod_info.audio_input != 0) continue;
                                        }

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
                channel->insert_module(modx::create_module(editor.song.audio_engine(), module_to_add), index_of_module_to_add);

            if (module_to_delete != -1)
                channel->remove_module(module_to_delete);
        }
        ImGui::EndChild();
    } ImGui::End();
}
