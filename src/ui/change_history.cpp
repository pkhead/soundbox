#include "change_history.h"
#include "../audio.h"
#include "editor.h"
#include <cstdlib>
#include <iostream>
#include <sstream>

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

void track_snapshot(change::ChangeSongMaxPatterns::TrackSnapshot& snapshot, Song* song)
{
    snapshot.rows = song->channels.size();
    snapshot.cols = song->length();
    snapshot.patterns.clear();
    snapshot.patterns.reserve(snapshot.rows * snapshot.cols);

    for (int row = 0; row < snapshot.rows; row++)
    {
        for (int col = 0; col < snapshot.cols; col++)
        {
            snapshot.patterns.push_back(song->channels[row]->sequence[col]);
        }
    }
}

void apply_snapshot(change::ChangeSongMaxPatterns::TrackSnapshot& snapshot, Song* song)
{
    int i = 0;

    for (int row = 0; row < snapshot.rows; row++)
    {
        for (int col = 0; col < snapshot.cols; col++)
        {
            song->channels[row]->sequence[col] = snapshot.patterns[i];
            i++;
        }
    }
}

change::ChangeSongMaxPatterns::ChangeSongMaxPatterns(int old_count, int new_count, Song* song)
    : old_count(old_count), new_count(new_count)
{
    track_snapshot(before, song);
}

void change::ChangeSongMaxPatterns::undo(SongEditor& editor) {
    track_snapshot(after, &editor.song);
    editor.song.set_max_patterns(old_count);
    apply_snapshot(before, &editor.song);
}

void change::ChangeSongMaxPatterns::redo(SongEditor& editor) {
    apply_snapshot(after, &editor.song);
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
    editor.selected_channel = channel_index;
    editor.song.channels[channel_index]->vol_mod.volume = old_vol;
}

void change::ChangeChannelVolume::redo(SongEditor& editor) {
    editor.selected_channel = channel_index;
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
    editor.selected_channel = channel_index;
    editor.song.channels[channel_index]->vol_mod.panning = old_val;
}

void change::ChangeChannelPanning::redo(SongEditor& editor) {
    editor.selected_channel = channel_index;
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
    editor.selected_channel = channel_index;
    editor.song.channels[channel_index]->set_fx_target(old_val);
}

void change::ChangeChannelOutput::redo(SongEditor& editor) {
    editor.selected_channel = channel_index;
    editor.song.channels[channel_index]->set_fx_target(new_val);
}

bool change::ChangeChannelOutput::merge(Action* other) {
    return false;
}


static void get_rack_info(
    change::FXRackTargetType target_type,
    SongEditor& editor,
    int target_index,
    audiomod::EffectsRack** rack,
    const char** parent_name
)
{
    if (target_type == change::FXRackTargetType::TargetChannel)
    {
        *rack = &editor.song.channels[target_index]->effects_rack;
        *parent_name = editor.song.channels[target_index]->name;
        editor.selected_channel = target_index;
    }
    else if (target_type == change::FXRackTargetType::TargetFXBus)
    {
        *rack = &editor.song.fx_mixer[target_index]->rack;
        *parent_name = editor.song.channels[target_index]->name;
    }
    else {
        std::cerr << "invalid target type\n";
        abort();
    }
}

// Add Effect //

change::ChangeAddEffect::ChangeAddEffect(int target_index, FXRackTargetType target_type, std::string mod_type)
    : target_index(target_index), target_type(target_type), mod_type(mod_type)
    {}

void change::ChangeAddEffect::undo(SongEditor& editor) {
    audiomod::EffectsRack* rack;
    const char* parent_name;
    get_rack_info(target_type, editor, target_index, &rack, &parent_name);

    // delete the module at the back
    editor.song.mutex.lock();

    audiomod::ModuleBase* mod = rack->remove(rack->modules.size() - 1);
    if (mod != nullptr) {
        editor.hide_module_interface(mod);
        delete mod;
    }
    
    editor.song.mutex.unlock();
}

void change::ChangeAddEffect::redo(SongEditor& editor) {
    audiomod::EffectsRack* rack;
    const char* parent_name;
    get_rack_info(target_type, editor, target_index, &rack, &parent_name);

    editor.song.mutex.lock();

    audiomod::ModuleBase* mod = audiomod::create_module(mod_type, &editor.song);
    mod->parent_name = parent_name;
    rack->insert(mod);

    editor.song.mutex.unlock();
}

bool change::ChangeAddEffect::merge(Action* other) {
    return false;
}

// Remove Effect //

change::ChangeRemoveEffect::ChangeRemoveEffect(int target_index, FXRackTargetType target_type, int index, audiomod::ModuleBase* mod)
    : target_index(target_index), target_type(target_type), index(index)
{
    mod_type = mod->id;
    
    // save module state
    std::stringstream stream;
    mod->save_state(stream);

    mod_state = stream.str();
}

void change::ChangeRemoveEffect::redo(SongEditor& editor) {
    audiomod::EffectsRack* rack;
    const char* parent_name;
    get_rack_info(target_type, editor, target_index, &rack, &parent_name);

    // delete the module at the back
    editor.song.mutex.lock();

    audiomod::ModuleBase* mod = rack->remove(index);
    if (mod != nullptr) {
        mod_type = mod->id;
    
        // save module state
        std::stringstream stream;
        mod->save_state(stream);

        mod_state = stream.str();

        editor.hide_module_interface(mod);
        delete mod;
    }
    
    editor.song.mutex.unlock();
}

void change::ChangeRemoveEffect::undo(SongEditor& editor) {
    audiomod::EffectsRack* rack;
    const char* parent_name;
    get_rack_info(target_type, editor, target_index, &rack, &parent_name);

    editor.song.mutex.lock();

    audiomod::ModuleBase* mod = audiomod::create_module(mod_type, &editor.song);
    mod->parent_name = parent_name;
    rack->insert(mod, index);

    // load module state
    std::stringstream stream(mod_state);
    mod->load_state(stream, mod_state.size());

    editor.song.mutex.unlock();
}

bool change::ChangeRemoveEffect::merge(Action* other) {
    return false;
}

// Swap Effect //

change::ChangeSwapEffect::ChangeSwapEffect(int target_index, FXRackTargetType target_type, int old_index, int new_index)
    : target_index(target_index), target_type(target_type), old_index(old_index), new_index(new_index)
{}

void change::ChangeSwapEffect::undo(SongEditor& editor)
{
    audiomod::EffectsRack* rack;
    const char* parent_name;
    get_rack_info(target_type, editor, target_index, &rack, &parent_name);

    editor.song.mutex.lock();

    int min = new_index > old_index ? old_index : new_index;
    int max = new_index > old_index ? new_index : old_index;

    audiomod::ModuleBase* mod0 = rack->remove(min);
    rack->insert(mod0, max);
    audiomod::ModuleBase* mod1 = rack->remove(max - 1);
    rack->insert(mod1, min);

    editor.song.mutex.unlock();
}

// same as undo
void change::ChangeSwapEffect::redo(SongEditor& editor)
{
    return undo(editor);
}

bool change::ChangeSwapEffect::merge(Action* other)
{
    return false;
}

// Add Note //
change::ChangeAddNote::ChangeAddNote(int channel_index, int bar, Song* song, bool from_null_pattern, int old_pattern_count, Note note)
    : channel_index(channel_index), bar(bar), note(note), from_null_pattern(from_null_pattern)
{
    new_index = song->channels[channel_index]->sequence[bar];
    old_max_patterns = old_pattern_count;
}

void change::ChangeAddNote::undo(SongEditor& editor)
{
    editor.song.mutex.lock();

    editor.selected_channel = channel_index;
    editor.selected_bar = bar;

    int pattern_id = editor.song.channels[channel_index]->sequence[bar] - 1;
    Pattern* pattern = editor.song.channels[channel_index]->patterns[pattern_id];

    for (auto it = pattern->notes.begin(); it != pattern->notes.end(); it++)
    {
        Note& note_v = *it;
        if (note_v == note)
        {
            pattern->notes.erase(it);
            break;
        }
    }

    if (from_null_pattern) {
        new_max_patterns = editor.song.max_patterns();
        new_index = pattern_id;

        editor.song.channels[channel_index]->sequence[bar] = 0;
        editor.song.set_max_patterns(old_max_patterns);
    }

    editor.song.mutex.unlock();
}

void change::ChangeAddNote::redo(SongEditor& editor)
{
    Song& song = editor.song;

    song.mutex.lock();
    
    editor.selected_channel = channel_index;
    editor.selected_bar = bar;
    
    Channel* channel = editor.song.channels[channel_index];
    int pattern_id = channel->sequence[bar] - 1;

    if (pattern_id == -1)
    {
        pattern_id = song.new_pattern(editor.selected_channel);
        channel->sequence[editor.selected_bar] = pattern_id + 1;
    }
    
    Pattern* pattern = channel->patterns[pattern_id];
    pattern->notes.push_back(note);

    song.mutex.unlock();
}

bool change::ChangeAddNote::merge(Action* other)
{
    return false;
}

// Remove Note //
change::ChangeRemoveNote::ChangeRemoveNote(int channel_index, int bar, Note note)
    : channel_index(channel_index), bar(bar), note(note)
{}

void change::ChangeRemoveNote::redo(SongEditor& editor)
{
    editor.song.mutex.lock();
    
    editor.selected_channel = channel_index;
    editor.selected_bar = bar;
    
    int pattern_id = editor.song.channels[channel_index]->sequence[bar] - 1;
    Pattern* pattern = editor.song.channels[channel_index]->patterns[pattern_id];

    for (auto it = pattern->notes.begin(); it != pattern->notes.end(); it++)
    {
        Note& note_v = *it;
        if (note_v == note)
        {
            pattern->notes.erase(it);
            break;
        }
    }

    editor.song.mutex.unlock();
}

void change::ChangeRemoveNote::undo(SongEditor& editor)
{
    editor.song.mutex.lock();
    
    editor.selected_channel = channel_index;
    editor.selected_bar = bar;
    
    int pattern_id = editor.song.channels[channel_index]->sequence[bar] - 1;
    Pattern* pattern = editor.song.channels[channel_index]->patterns[pattern_id];
    pattern->notes.push_back(note);

    editor.song.mutex.unlock();
}

bool change::ChangeRemoveNote::merge(Action* other)
{
    return false;
}

// Change Note //
change::ChangeNote::ChangeNote(int channel_index, int bar, Note old_note, Note new_note)
    : channel_index(channel_index), bar(bar), old_note(old_note), new_note(new_note)
{}

void change::ChangeNote::undo(SongEditor& editor)
{
    editor.song.mutex.lock();
    
    editor.selected_channel = channel_index;
    editor.selected_bar = bar;
    
    int pattern_id = editor.song.channels[channel_index]->sequence[bar] - 1;
    Pattern* pattern = editor.song.channels[channel_index]->patterns[pattern_id];

    for (Note& note_v : pattern->notes)
    {
        if (note_v == new_note)
        {
            note_v = old_note;
            break;
        }
    }

    editor.song.mutex.unlock();
}

void change::ChangeNote::redo(SongEditor& editor)
{
    editor.song.mutex.lock();
    
    editor.selected_channel = channel_index;
    editor.selected_bar = bar;
    
    int pattern_id = editor.song.channels[channel_index]->sequence[bar] - 1;
    Pattern* pattern = editor.song.channels[channel_index]->patterns[pattern_id];

    for (Note& note_v : pattern->notes)
    {
        if (note_v == old_note)
        {
            note_v = new_note;
            break;
        }
    }

    editor.song.mutex.unlock();
}

bool change::ChangeNote::merge(Action* other)
{
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

// Insert Bar //
change::ChangeInsertBar::ChangeInsertBar(int bar)
    : pos(bar), count(1)
{}

void change::ChangeInsertBar::undo(SongEditor& editor)
{
    editor.selected_bar = pos - 1;
    if (editor.selected_bar < 0) editor.selected_bar = 0;

    for (int i = 0; i < count; i++)
        editor.song.remove_bar(pos);
}

void change::ChangeInsertBar::redo(SongEditor& editor)
{
    for (int i = 0; i < count; i++)
        editor.song.insert_bar(pos);
}

bool change::ChangeInsertBar::merge(Action* other)
{
    return false;

    // TODO: merge if changes are contiguous
    /*if (get_type() != other->get_type()) return false;
    ChangeInsertBar* sub = static_cast<ChangeInsertBar*>(other);
    
    // if new change is one unit left of old change
    if (sub->pos + sub->count == pos + 1)
    {
        std::cout << "left\n";

        pos = sub->pos;
        count = sub->count + 1;
        return true;
    }

    // if new change is one unit right of old change
    else if (sub->pos == pos + count)
    {
        count += sub->count;
        return true;
    }

    // bars are not touching, do not merge
    return false;*/
}

// Remove Bar //
change::ChangeRemoveBar::ChangeRemoveBar(int bar, Song* song)
{
    bars.push_back({
        bar,
        song->get_bar_patterns(bar)
    });
}

void change::ChangeRemoveBar::undo(SongEditor& editor)
{
    for (const Bar& bar : bars)
    {
        editor.song.insert_bar(bar.pos);
        editor.song.set_bar_patterns(bar.pos, bar.pattern_indices);
        editor.selected_bar = bar.pos;
    }
}

void change::ChangeRemoveBar::redo(SongEditor& editor)
{
    for (Bar& bar : bars)
    {
        bar.pattern_indices = std::move(editor.song.get_bar_patterns(bar.pos));
        editor.song.remove_bar(bar.pos);
        editor.selected_bar = bar.pos - 1;
        if (editor.selected_bar < 0) editor.selected_bar = 0;
    }
}

bool change::ChangeRemoveBar::merge(Action* other)
{
    return false;

    /* TODO: merge if contiguous
    if (get_type() != other->get_type()) return false;
    ChangeRemoveBar* sub = static_cast<ChangeRemoveBar*>(other);
    
    for (const Bar& bar : sub->bars)
    {
        bars.push_back(bar);
    }*/
}

// Change Sequence //
change::ChangeSequence::ChangeSequence(int channel, int bar, int old_val, int new_val)
    : channel(channel), bar(bar), old_val(old_val), new_val(new_val)
{}

void change::ChangeSequence::undo(SongEditor& editor)
{
    editor.selected_channel = channel;
    editor.selected_bar = bar;
    editor.song.channels[channel]->sequence[bar] = old_val;
}

void change::ChangeSequence::redo(SongEditor& editor)
{
    editor.selected_channel = channel;
    editor.selected_bar = bar;
    editor.song.channels[channel]->sequence[bar] = new_val;
}

bool change::ChangeSequence::merge(Action* other)
{
    if (get_type() != other->get_type()) return false;
    ChangeSequence* sub = static_cast<ChangeSequence*>(other);

    // if same cell
    if (channel == sub->channel && bar == sub->bar)
    {
        new_val = sub->new_val;
        return true;
    }

    return false;
}