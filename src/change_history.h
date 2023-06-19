#pragma once

#include <cstdint>
#include <imgui.h>
#include <vector>
#include "song.h"

enum class ChangeActionType : uint8_t
{
    Unknown = 0,
    SongTempo,
    SongMaxPatterns,
    ChannelVolume,
    ChannelPanning,
};

class ChangeAction
{
public:
    virtual ~ChangeAction() {};

    virtual bool get_type() const;
    virtual void undo();
    virtual void redo();
    virtual bool can_merge(const ChangeAction& other) const;
};

class ChangeSongTempo : ChangeAction
{
public:
    ChangeSongTempo(float old_tempo, float new_tempo);

    bool get_type() const override;
    void undo() override;
    void redo() override;
    bool can_merge(const ChangeAction& other) const override;
};

class ChangeQueue
{
private:
    std::vector<ChangeAction> changes;
public:
    ~ChangeQueue();

    void push(const ChangeAction&& action);
    ChangeAction pop();
    void clear();
};