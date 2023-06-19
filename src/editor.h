#pragma once
#include "audio.h"
#include "imgui.h"
#include "song.h"

class SongEditor;

namespace change
{
    enum ActionType : uint8_t
    {
        Unknown = 0,
        SongTempo,
        SongMaxPatterns,
        ChannelVolume,
        ChannelPanning,
    };

    class Action
    {
    public:
        virtual ~Action() {};

        virtual ActionType get_type() const = 0;
        virtual void undo(SongEditor& editor) = 0;
        virtual void redo(SongEditor& editor) = 0;
        virtual bool merge(Action* other) = 0;
    };

    class ChangeSongTempo : public Action
    {
    public:
        ChangeSongTempo(float old_tempo, float new_tempo);

        float old_tempo, new_tempo;

        ActionType get_type() const override { return SongTempo; };
        void undo(SongEditor& editor) override;
        void redo(SongEditor& editor) override;
        bool merge(Action* other) override;
    };

    class Queue
    {
    private:
        std::vector<Action*> changes;
    public:
        ~Queue();

        void push(Action* action);
        Action* pop();
        void clear();
    };
}

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
    std::unordered_map<ImGuiID, void*> ui_values;

    // returns true if there was something to undo
    bool undo();

    // returns true if there was something to redo
    bool redo();

    change::Queue undo_queue;
    change::Queue redo_queue;

    void delete_fx_bus(audiomod::FXBus* bus_to_delete);
    void toggle_module_interface(audiomod::ModuleBase* mod);
    void hide_module_interface(audiomod::ModuleBase* mod);
};