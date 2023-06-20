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

// Song Max Patterns //

change::ChangeSongMaxPatterns::ChangeSongMaxPatterns(int old_count, int new_count)
    : old_count(old_count), new_count(new_count)
    {}

void change::ChangeSongMaxPatterns::undo(SongEditor& editor) {
    editor.song.set_max_patterns(old_count);
}

void change::ChangeSongMaxPatterns::redo(SongEditor& editor) {
    editor.song.set_max_patterns(new_count);
}

bool change::ChangeSongMaxPatterns::merge(change::Action* other)
{
    if (get_type() != other->get_type()) return false;
    ChangeSongMaxPatterns* sub = static_cast<ChangeSongMaxPatterns*>(other);
    new_count = sub->new_count;
    return true;
}

// Channel Volume //

change::ChangeChannelVolume::ChangeChannelVolume(int channel_index, float old_val, float new_val)
    : channel_index(channel_index), old_vol(old_val), new_vol(new_val)
    {}

void change::ChangeChannelVolume::undo(SongEditor& editor) {
    editor.song.channels[channel_index]->vol_mod.volume = old_vol;
}

void change::ChangeChannelVolume::redo(SongEditor& editor) {
    editor.song.channels[channel_index]->vol_mod.volume = new_vol;
}

bool change::ChangeChannelVolume::merge(Action* other) {
    if (get_type() != other->get_type()) return false;
    ChangeChannelVolume* sub = static_cast<ChangeChannelVolume*>(other);
    if (sub->channel_index != channel_index) return false;
    new_vol = sub->new_vol;
    return true;
}

// Channel Panning //

change::ChangeChannelPanning::ChangeChannelPanning(int channel_index, float old_val, float new_val)
    : channel_index(channel_index), old_val(old_val), new_val(new_val)
    {}

void change::ChangeChannelPanning::undo(SongEditor& editor) {
    editor.song.channels[channel_index]->vol_mod.panning = old_val;
}

void change::ChangeChannelPanning::redo(SongEditor& editor) {
    editor.song.channels[channel_index]->vol_mod.panning = new_val;
}

bool change::ChangeChannelPanning::merge(Action* other) {
    if (get_type() != other->get_type()) return false;
    ChangeChannelPanning* sub = static_cast<ChangeChannelPanning*>(other);
    if (sub->channel_index != channel_index) return false;
    new_val = sub->new_val;
    return true;
}

// Channel Output //

change::ChangeChannelOutput::ChangeChannelOutput(int channel_index, int old_val, int new_val)
    : channel_index(channel_index), old_val(old_val), new_val(new_val)
    {}

void change::ChangeChannelOutput::undo(SongEditor& editor) {
    editor.song.channels[channel_index]->set_fx_target(old_val);
}

void change::ChangeChannelOutput::redo(SongEditor& editor) {
    editor.song.channels[channel_index]->set_fx_target(new_val);
}

bool change::ChangeChannelOutput::merge(Action* other) {
    return false;
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