#include "ui.h"
#include "../modules/modules.h"

using namespace ui;

void ui::render_channel_settings(SongEditor &editor)
{
    Song& song = *editor.song;
    static char char_buf[64];
    
    auto& cur_channel = song.channels[editor.selected_channel];

    if (ImGui::Begin("Channel Settings")) {
        // channel name
        ImGui::PushItemWidth(-1.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Name");
        ImGui::SameLine();
        ImGui::InputText("##channel_name", cur_channel->name, 16);

        auto& vol_mod = cur_channel->vol_mod->module<audiomod::VolumeModule>();

        // volume slider
        {
            float volume = vol_mod.volume * 100.0f;
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Volume");
            ImGui::SameLine();
            ImGui::SliderFloat("##channel_volume", &volume, 0.0f, 100.0f, "%.0f");
            if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) volume = 50.0f;
            vol_mod.volume = volume / 100.0f;

            // change detection
            {
                // make id
                snprintf(char_buf, 64, "chvol%i", editor.selected_channel);

                float prev, cur;
                cur = vol_mod.volume;
                if (change_detection(editor, cur, &prev, ImGui::GetID(char_buf))) {
                    editor.push_change(new change::ChangeChannelVolume(editor.selected_channel, prev, cur));
                }
            }
        }

        // panning slider
        {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Panning");
            ImGui::SameLine();
            ImGui::SliderFloat("##channel_panning", &vol_mod.panning, -1, 1, "%.2f");
            if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) vol_mod.panning = 50.0f;
            
            // change detection
            {
                // make id
                snprintf(char_buf, 64, "chpan%i", editor.selected_channel);

                float prev, cur;
                cur = vol_mod.panning;
                if (change_detection(editor, cur, &prev, ImGui::GetID(char_buf))) {
                    editor.push_change(new change::ChangeChannelPanning(editor.selected_channel, prev, cur));
                }
            }
        }

        {
            // fx mixer bus combobox
            ImGui::AlignTextToFramePadding();
            ImGui::Text("FX Output");
            ImGui::SameLine();

            // write preview value
            auto& cur_fx_bus = song.fx_mixer[cur_channel->fx_target_idx];
            snprintf(char_buf, 64, "%i - %s", cur_channel->fx_target_idx, cur_fx_bus->name);

            if (ImGui::BeginCombo("##channel_fx_target", char_buf))
            {
                // list potential targets
                for (size_t target_i = 0; target_i < song.fx_mixer.size(); target_i++)
                {
                    auto& target_bus = song.fx_mixer[target_i];

                    // write target bus name
                    snprintf(char_buf, 64, "%lu - %s", target_i, target_bus->name);

                    bool is_selected = target_i == cur_channel->fx_target_idx;
                    if (ImGui::Selectable(char_buf, is_selected))
                        cur_channel->set_fx_target(target_i);

                    if (is_selected) ImGui::SetItemDefaultFocus();

                    // change detection
                    {
                        // make id
                        snprintf(char_buf, 64, "chout%i", editor.selected_channel);

                        int prev, cur;
                        cur = cur_channel->fx_target_idx;
                        if (change_detection(editor, cur, &prev, ImGui::GetID(char_buf))) {
                            editor.push_change(new change::ChangeChannelOutput(editor.selected_channel, prev, cur));
                        }
                    }
                }

                ImGui::EndCombo();

            }
        }

        ImGui::PopItemWidth();
        ImGui::NewLine();

        // load instrument
        ImGui::Text("Instrument: %s", cur_channel->synth_mod->module().name.c_str());
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
        }
        
    } ImGui::End();
}