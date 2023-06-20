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
        ChannelOutput
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
        ChangeSongTempo(ImGuiID id, float old_tempo, float new_tempo);

        ImGuiID id;
        float old_tempo, new_tempo;

        ActionType get_type() const override { return ActionType::SongTempo; };
        void undo(SongEditor& editor) override;
        void redo(SongEditor& editor) override;
        bool merge(Action* other) override;
    };

    class ChangeSongMaxPatterns : public Action
    {
    public:
        ChangeSongMaxPatterns(int old_count, int new_count);
        int old_count, new_count;

        ActionType get_type() const override { return ActionType::SongMaxPatterns; };
        void undo(SongEditor& editor) override;
        void redo(SongEditor& editor) override;
        bool merge(Action* other) override;
    };

    class ChangeChannelVolume : public Action
    {
    public:
        ChangeChannelVolume(int channel_index, float old_vol, float new_vol);
        int channel_index;
        float old_vol, new_vol;

        ActionType get_type() const override { return ActionType::ChannelVolume; };
        void undo(SongEditor& editor) override;
        void redo(SongEditor& editor) override;
        bool merge(Action* other) override;
    };

    class ChangeChannelPanning : public Action
    {
    public:
        ChangeChannelPanning(int channel_index, float old_val, float new_val);
        int channel_index;
        float old_val, new_val;

        ActionType get_type() const override { return ActionType::ChannelPanning; };
        void undo(SongEditor& editor) override;
        void redo(SongEditor& editor) override;
        bool merge(Action* other) override;
    };

    class ChangeChannelOutput : public Action
    {
    public:
        ChangeChannelOutput(int channel_index, int old_val, int new_val);
        int channel_index;
        int old_val, new_val;

        ActionType get_type() const override { return ActionType::ChannelOutput; };
        void undo(SongEditor& editor) override;
        void redo(SongEditor& editor) override;
        bool merge(Action* other) override;
    };

    class Stack
    {
    private:
        std::vector<Action*> changes;
    public:
        ~Stack();

        inline bool is_empty() const { return changes.empty(); }
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

    // push a change to the undo stack
    inline void push_change(change::Action* action) {
        undo_stack.push(action);
        redo_stack.clear();    
    };

    change::Stack undo_stack;
    change::Stack redo_stack;

    void delete_fx_bus(audiomod::FXBus* bus_to_delete);
    void toggle_module_interface(audiomod::ModuleBase* mod);
    void hide_module_interface(audiomod::ModuleBase* mod);
};