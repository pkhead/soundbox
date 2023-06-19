#pragma once
#include "audio.h"
#include "song.h"

class SongEditor {
private:

public:
    SongEditor(Song& song);
    ~SongEditor();
    Song& song;

    int selected_channel = 0;
    int selected_bar = 0;

    // TODO: use system clipboard
    std::vector<Note> note_clipboard;

    float quantization = 0.25f;
    bool show_tuning_window = false;
    std::vector<audiomod::ModuleBase*> mod_interfaces;
    std::vector<audiomod::FXBus*> fx_interfaces;

    // the previous values of monitored UI inputs
    //std::unordered_map<void*, void*> ui_values;

    // returns true if there was something to undo
    //bool undo();

    // returns true if there was something to redo
    //bool redo();

    void delete_fx_bus(audiomod::FXBus* bus_to_delete);
    void toggle_module_interface(audiomod::ModuleBase* mod);
    void hide_module_interface(audiomod::ModuleBase* mod);
};