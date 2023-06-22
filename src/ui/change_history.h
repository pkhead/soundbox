#pragma once
#include "../audio.h"
#include "../song.h"
#include <cstdint>
#include <imgui.h>
#include <vector>
#include <string>

class SongEditor;

namespace change
{
    enum ActionType : uint8_t
    {
        Unknown = 0,
        SongTempo,
        SongMaxPatterns,
        
        NewChannel,
        RemoveChannel,
        ChannelVolume,
        ChannelPanning,
        ChannelOutput,

        AddEffect,
        RemoveEffect,
        ModifyEffect, // TODO
        SwapEffect,
        AddBus, // TODO
        RemoveBus, // TODO

        NoteAdd,
        NoteRemove,
        NoteChange,

        NewPattern, // TODO
        SequenceChange,
        InsertBar,
        RemoveBar,
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
        ChangeSongMaxPatterns(int old_count, int new_count, Song* song);
        int old_count, new_count;

        struct TrackSnapshot
        {
            int rows;
            int cols;
            std::vector<int> patterns;
        } before, after; // snapshots of patterns before change

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

    enum class FXRackTargetType {
        TargetChannel,
        TargetFXBus
    };

    class ChangeAddEffect : public Action
    {
    public:
        ChangeAddEffect(int target_index, FXRackTargetType target_type, std::string mod_type);
        int target_index;
        FXRackTargetType target_type;
        std::string mod_type;

        ActionType get_type() const override { return ActionType::AddEffect; };
        void undo(SongEditor& editor) override;
        void redo(SongEditor& editor) override;
        bool merge(Action* other) override;
    };

    class ChangeRemoveEffect : public Action
    {
    public:
        ChangeRemoveEffect(int target_index, FXRackTargetType target_type, int index, audiomod::ModuleBase* mod);
        int target_index;
        FXRackTargetType target_type;
        int index;
        std::string mod_type;
        std::string mod_state;

        ActionType get_type() const override { return ActionType::RemoveEffect; };
        void undo(SongEditor& editor) override;
        void redo(SongEditor& editor) override;
        bool merge(Action* other) override;
    };

    class ChangeSwapEffect : public Action
    {
    public:
        ChangeSwapEffect(int target_index, FXRackTargetType target_type, int old_index, int new_index);
        int target_index;
        FXRackTargetType target_type;
        int old_index, new_index;

        ActionType get_type() const override { return ActionType::SwapEffect; };
        void undo(SongEditor& editor) override;
        void redo(SongEditor& editor) override;
        bool merge(Action* other) override;
    };

    class ChangeAddNote : public Action
    {
    public:
        ChangeAddNote(int channel_index, int bar, Song* song, bool from_null_pattern, int old_pattern_count, Note note);
        int channel_index, bar;
        Note note;

        // if clicked on a null pattern, index of selected cell changes
        bool from_null_pattern;
        int new_index;

        // if there were no empty patterns on a null pattern, action increases pattern count by 1
        int old_max_patterns;
        int new_max_patterns;

        ActionType get_type() const override { return ActionType::NoteAdd; };
        void undo(SongEditor& editor) override;
        void redo(SongEditor& editor) override;
        bool merge(Action* other) override;
    };

    class ChangeRemoveNote : public Action
    {
    public:
        ChangeRemoveNote(int channel_index, int bar, Note note);
        int channel_index, bar;
        Note note;

        ActionType get_type() const override { return ActionType::NoteRemove; };
        void undo(SongEditor& editor) override;
        void redo(SongEditor& editor) override;
        bool merge(Action* other) override;
    };

    class ChangeNote : public Action
    {
    public:
        ChangeNote(int channel_index, int bar, Note old_note, Note new_note);
        int channel_index, bar;
        Note old_note, new_note;

        ActionType get_type() const override { return ActionType::NoteChange; };
        void undo(SongEditor& editor) override;
        void redo(SongEditor& editor) override;
        bool merge(Action* other) override;
    };

    class ChangeInsertBar : public Action
    {
    public:
        ChangeInsertBar(int bar);
        int pos;
        int count;

        ActionType get_type() const override { return ActionType::InsertBar; };
        void undo(SongEditor& editor) override;
        void redo(SongEditor& editor) override;
        bool merge(Action* other) override;
    };

    class ChangeRemoveBar : public Action
    {
    public:
        struct Bar
        {
            int pos;
            std::vector<int> pattern_indices;
        };

        ChangeRemoveBar(int bar, Song* song);
        int bar;
        std::vector<Bar> bars;

        ActionType get_type() const override { return ActionType::RemoveBar; };
        void undo(SongEditor& editor) override;
        void redo(SongEditor& editor) override;
        bool merge(Action* other) override;
    };

    class ChangeSequence : public Action
    {
    public:
        ChangeSequence(int channel, int bar, int old_val, int new_val);
        int channel, bar, old_val, new_val;

        ActionType get_type() const override { return ActionType::SequenceChange; };
        void undo(SongEditor& editor) override;
        void redo(SongEditor& editor) override;
        bool merge(Action* other) override;
    };

    struct ModuleData
    {
        const char* type;
        std::string data;

        ModuleData(audiomod::ModuleBase* module);
        audiomod::ModuleBase* load(Song* song) const;
    };

    class ChangeNewChannel : public Action
    {
    public:
        ChangeNewChannel(int index);

        int index;

        ActionType get_type() const override { return ActionType::NewChannel; };
        void undo(SongEditor& editor) override;
        void redo(SongEditor& editor) override;
        bool merge(Action* other) override;
    };

    class ChangeRemoveChannel : public Action
    {
    private:
        void _save(Channel* channel);
    
    public:
        ChangeRemoveChannel(int index, Channel* channel);

        int index;
        int fx_target;
        bool solo;
        std::string name;
        std::vector<int> sequence;
        std::vector<Pattern> patterns;

        std::string vol_mod_data;

        ModuleData instrument;
        std::vector<ModuleData> effects;

        ActionType get_type() const override { return ActionType::RemoveChannel; };
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