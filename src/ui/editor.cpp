#include "editor.h"
#include "../audio.h"

SongEditor::SongEditor(Song& song) :
    song(song)
{
    
}

SongEditor::~SongEditor()
{
    for (auto it : ui_values)
        free(it.second);
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

    song.delete_fx_bus(bus_to_delete);
}