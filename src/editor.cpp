#include "editor.h"
#include "audio.h"

////////////////////
// CHANGE ACTIONS //
////////////////////

// Song Tempo //

change::ChangeSongTempo::ChangeSongTempo(ImGuiID id, float old_tempo, float new_tempo)
    : old_tempo(old_tempo), new_tempo(new_tempo), id(id)
    {}

void change::ChangeSongTempo::undo(SongEditor& editor) {
    editor.song.tempo = old_tempo;
    memcpy(editor.ui_values[id], &old_tempo, sizeof(old_tempo));
}

void change::ChangeSongTempo::redo(SongEditor& editor) {
    editor.song.tempo = new_tempo;
    memcpy(editor.ui_values[id], &old_tempo, sizeof(old_tempo));
}

bool change::ChangeSongTempo::merge(change::Action* other)
{
    if (get_type() != other->get_type()) return false;
    ChangeSongTempo* sub = static_cast<ChangeSongTempo*>(other);
    new_tempo = sub->new_tempo;
    return true;
}

//////////////////
// CHANGE QUEUE //
//////////////////

change::Stack::~Stack()
{
    clear();
}

void change::Stack::push(change::Action* change_action)
{
    // merge change if possible
    if (!changes.empty() && changes.back()->merge(change_action))
    {
        delete change_action;
        return;
    }
    else
    {
        changes.push_back(change_action);
    }
}

change::Action* change::Stack::pop()
{
    Action* action = changes.back();
    changes.pop_back();
    return action;
}

void change::Stack::clear()
{
    for (Action* action : changes)
    {
        delete action;
    }

    changes.clear();
}

/////////////////
// SONG EDITOR //
/////////////////

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