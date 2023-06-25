#include "ui.h"

void render_fx_mixer(SongEditor &editor)
{
    ImGuiStyle& style = ImGui::GetStyle();
    Song& song = *editor.song;

    if (ImGui::Begin("FX Mixer"))
    {
        int i = 0;

        float frame_width = ImGui::CalcTextSize("MMMMMMMMMMMMMMMM").x + style.FramePadding.x;
        
        float mute_btn_size = ImGui::CalcTextSize("M").x + style.FramePadding.x * 2.0f;
        float solo_btn_size = ImGui::CalcTextSize("S").x + style.FramePadding.x * 2.0f;

        audiomod::FXBus* bus_to_delete = nullptr;

        for (audiomod::FXBus* bus : song.fx_mixer)
        {
            ImGui::PushID(i);

            float width = ImGui::GetContentRegionAvail().x;
            
            std::string label = std::to_string(i) + " - " + bus->name;
            if (ImGui::Selectable(
                label.c_str(),
                bus->interface_open,
                0,
                Vec2(width - mute_btn_size - solo_btn_size - style.ItemSpacing.x * 2.0f, 0.0f)
            ))
            // if selectable is clicked,
                bus->interface_open = !bus->interface_open;

            // right click to remove
            if (i != 0 && ImGui::BeginPopupContextItem())
            {
                if (ImGui::Selectable("Remove", false))
                    bus_to_delete = bus;

                ImGui::EndPopup();
            }

            // mute button
            push_btn_disabled(style, !bus->controller.mute);
            ImGui::SameLine();
            if (ImGui::SmallButton("M"))
            {
                bus->controller.mute = !bus->controller.mute;
            }
            pop_btn_disabled();

            // solo button
            push_btn_disabled(style, !bus->solo);
            ImGui::SameLine();
            if (ImGui::SmallButton("S"))
            {
                bus->solo = !bus->solo;
            }
            pop_btn_disabled();

            // left channel
            ImGui::ProgressBar(bus->controller.analysis_volume[0], Vec2(-1.0f, 1.0f), "");
            // right channel
            ImGui::ProgressBar(bus->controller.analysis_volume[1], Vec2(-1.0f, 1.0f), "");

            ImGui::PopID();
            
            i++;
        }

        // delete requested bus
        if (bus_to_delete)
        {
            song.mutex.lock();
            song.delete_fx_bus(bus_to_delete);
            song.mutex.unlock();
        }

        ImGui::Separator();
        if (ImGui::Button("Add", ImVec2(-1.0f, 0.0f)))
        {
            song.mutex.lock();

            audiomod::FXBus* bus = new audiomod::FXBus;
            song.fx_mixer[0]->connect_input(&bus->controller);
            song.fx_mixer.push_back(bus);

            song.mutex.unlock();
        }
    } ImGui::End();

    // render fx interfaces
    static char char_buf[64];
    int i = 0;
    audiomod::FXBus* bus_to_delete = nullptr;

    for (audiomod::FXBus* fx_bus : song.fx_mixer)
    {
        if (!fx_bus->interface_open)
        {
            i++;
            continue;
        }
        
        // write window id
        snprintf(char_buf, 64, "FX: %i - %s###%p", i, fx_bus->name, fx_bus);
        
        if (ImGui::Begin(
            char_buf, // window id
            &fx_bus->interface_open,
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking
        )) {
            // set next item width to make sure window title is not truncated
            ImGui::SetNextItemWidth(ImGui::CalcTextSize("FX: 0 MM - MMMMMMMMMMMMMMMMMM").x);
            ImGui::InputText("##name", fx_bus->name, fx_bus->name_capacity);

            // show delete button
            if (i > 0)
            {
                ImGui::SameLine();
                if (ImGui::Button("Remove"))
                    ImGui::OpenPopup("Remove");

                if (ImGui::BeginPopup("Remove")) {
                    if (ImGui::Selectable("Confirm?"))
                        bus_to_delete = fx_bus;

                    ImGui::EndPopup();
                }

                // show help explaining alternative method to remove buses
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
                    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 20.0f);
                    
                    ImGui::TextWrapped(
                        "You can also remove FX buses by right-clicking on them "
                        "in the bus list. "
                    );
                    
                    ImGui::PopTextWrapPos();
                    ImGui::EndTooltip();
                }
            }

            // show volume analysis and TODO: gain slider
            // left channel
            float bar_height = ImGui::GetTextLineHeight() * 0.25f;
            ImGui::ProgressBar(fx_bus->controller.analysis_volume[0], Vec2(-1.0f, bar_height), "");
            // right channel
            ImGui::ProgressBar(fx_bus->controller.analysis_volume[1], Vec2(-1.0f, bar_height), "");

            // show output bus combobox
            if (i > 0)
            {
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Output Bus");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(-1.0f);
                // write preview value
                snprintf(char_buf, 64, "%i - %s", fx_bus->target_bus, song.fx_mixer[fx_bus->target_bus]->name);

                if (ImGui::BeginCombo("##fx_target", char_buf))
                {
                    // list potential targets
                    for (size_t target_i = 0; target_i < song.fx_mixer.size(); target_i++)
                    {
                        audiomod::FXBus* target_bus = song.fx_mixer[target_i];

                        // skip if looking at myself or an input of myself
                        if (target_bus == fx_bus || song.fx_mixer[target_bus->target_bus] == fx_bus) continue;

                        // write target bus name
                        snprintf(char_buf, 64, "%lu - %s", target_i, target_bus->name);

                        bool is_selected = target_i == fx_bus->target_bus;
                        if (ImGui::Selectable(char_buf, is_selected))
                        {
                            // remove old connection
                            song.fx_mixer[fx_bus->target_bus]->disconnect_input(&fx_bus->controller);

                            // create new connection
                            fx_bus->target_bus = target_i;
                            target_bus->connect_input(&fx_bus->controller);
                        }

                        if (is_selected) ImGui::SetItemDefaultFocus();
                    }
                    
                    ImGui::EndCombo();
                }
            }

            EffectsInterfaceResult result;
            switch (effect_rack_ui(&editor, &fx_bus->rack, &result))
            {
                case EffectsInterfaceAction::Add: {
                    song.mutex.lock();
                    
                    audiomod::ModuleBase* mod = audiomod::create_module(result.module_id, editor.audio_dest, editor.plugin_manager);
                    mod->parent_name = fx_bus->name;
                    mod->song = &song;
                    fx_bus->insert(mod);

                    // register change
                    editor.push_change(new change::ChangeAddEffect(
                        i,
                        change::FXRackTargetType::TargetFXBus,
                        result.module_id
                    ));

                    song.mutex.unlock();
                    break;
                }

                case EffectsInterfaceAction::Edit:
                    editor.toggle_module_interface(fx_bus->rack.modules[result.target_index]);
                    break;

                case EffectsInterfaceAction::Delete: {
                    // delete the selected module
                    song.mutex.lock();

                    audiomod::ModuleBase* mod = fx_bus->rack.remove(result.target_index);
                    if (mod != nullptr) {
                        // register change
                        editor.push_change(new change::ChangeRemoveEffect(
                            i,
                            change::FXRackTargetType::TargetFXBus,
                            result.target_index,
                            mod
                        ));

                        editor.hide_module_interface(mod);
                        mod->release();
                    }

                    song.mutex.unlock();
                    break;
                }

                case EffectsInterfaceAction::Swapped:
                    editor.push_change(new change::ChangeSwapEffect(
                        i,
                        change::FXRackTargetType::TargetFXBus,
                        result.swap_start,
                        result.swap_end
                    ));
                    
                    break;

                case EffectsInterfaceAction::Nothing: break;
            }
        }

        ImGui::End();

        i++;
    }

    if (bus_to_delete)
        song.delete_fx_bus(bus_to_delete);
}