#include "editor.h"
#include "../audio.h"
#include "theme.h"

SongEditor::SongEditor(Song* song) :
    song(song)
{
    const char* theme_name = "Soundbox Dark";

    try
    {
        theme = Theme(theme_name);
    } catch(...) {
        std::cerr << "error: could not find theme \"" << theme_name << "\"\n";
    }

    theme.set_imgui_colors();
    reset();
}

SongEditor::~SongEditor()
{
    delete song;

    for (auto it : ui_values)
        free(it.second);
}

void SongEditor::reset()
{
    undo_stack.clear();
    redo_stack.clear();
    ui_values.clear();
    selected_channel = 0;
    selected_bar = 0;
}

bool SongEditor::undo()
{
    if (undo_stack.is_empty()) return false;

    change::Action* action = undo_stack.pop();
    action->undo(*this);
    redo_stack.push(action);
    return true;
}

bool SongEditor::redo()
{
    if (redo_stack.is_empty()) return false;

    change::Action* action = redo_stack.pop();
    action->redo(*this);
    undo_stack.push(action);
    return true;
}

void SongEditor::hide_module_interface(audiomod::ModuleBase* mod) {
    for (auto it = mod_interfaces.begin(); it != mod_interfaces.end(); it++) {
        if (*it == mod) {
            mod_interfaces.erase(it);
            break;
        }
    }
}

void SongEditor::toggle_module_interface(audiomod::ModuleBase* mod) {
    mod->show_interface = !mod->show_interface;
    
    // if want to show interface, add module to interfaces list
    if (mod->show_interface) {
        mod_interfaces.push_back(mod);

    // if want to hide interface, remove module from interfaces list
    } else {
        hide_module_interface(mod);
    }
}

void SongEditor::delete_fx_bus(audiomod::FXBus* bus_to_delete)
{
    // remove interface
    bus_to_delete->interface_open = false;
    for (auto it = fx_interfaces.begin(); it != fx_interfaces.end(); it++)
        if (*it == bus_to_delete) {
            fx_interfaces.erase(it);
            break;
        }

    song->delete_fx_bus(bus_to_delete);
}

void SongEditor::remove_channel(int channel_index)
{
    Channel* channel = song->channels[channel_index];

    // remove interfaces
    for (audiomod::ModuleBase* mod : channel->effects_rack.modules)
    {
        hide_module_interface(mod);
    }

    song->remove_channel(channel_index);
}