#include "change_history.h"
#include "song.h"
#include <cstdlib>
#include <cstring>

//////////////////
// change queue //
//////////////////

ChangeAction::ChangeAction(ChangeActionType type) : type(type) {
    switch (type)
    {
        case ChangeActionType::Unknown:
        case ChangeActionType::ChannelVolume:
        case ChangeActionType::ChannelPanning:

            data_type = ChangeActionData::Float;
            break;

        case CHan
    }
}

// copy constructor
enum class ChangeActionTarget : uint8_t
{
    Unknown = 0,
    SongTempo,
    SongMaxPatterns,
    ChannelVolume,
    ChannelPanning,
};

ChangeAction::ChangeAction(const ChangeAction& src)
{
    std::cout << "copy constructor";

    target = src.target;

    switch (src.target)
    {
        case ChangeActionTarget::SongTempo;
        
        case ChangeActionData::Float:
            type_float = src.type_float;
            break;

        case ChangeActionData::Int:
            type_int = src.type_int;
            break;

        case ChangeActionData::Note:
            type_note = src.type_note;
            break;

        case ChangeActionData::Bar:
            type_bar = src.type_bar;
            break;

        case ChangeActionData::Channel:
            type_channel = src.type_channel;
            break;
    }
}

// move constructor
ChangeAction::ChangeAction(ChangeAction&& src) noexcept
{
    std::cout << "move constructor";

    type = src.type;
    data_type = src.data_type;

    switch (src.data_type)
    {
        case ChangeActionData::Float:
            type_float = std::move(src.type_float);
            break;

        case ChangeActionData::Int:
            type_int = std::move(src.type_int);
            break;

        case ChangeActionData::Note:
            type_note = std::move(src.type_note);
            break;

        case ChangeActionData::Bar:
            type_bar = std::move(src.type_bar);
            break;

        case ChangeActionData::Channel:
            type_channel = std::move(src.type_channel);
            break;
    }
}

bool ChangeAction::can_merge(const ChangeAction& other) const
{
    switch (type)
    {
        // these types are not mergeable
        case ChangeActionType::Unknown: return false;
        // case ChangeActionType::NoteAdd: return false;
        // case ChangeActionType::NoteRemove: return false;
        // case ChangeActionType::ChannelAdd: return false;
        // case ChangeActionType::ChannelRemove: return false;

        default: return type == other.type;
    }
}

ChangeQueue::~ChangeQueue()
{
    clear();
}

void ChangeQueue::push(const ChangeAction&& change_action)
{
    // don't register change if mergeable
    if (!changes.empty() && change_action.can_merge(changes.back()))
    {
        return;
    }
    else
    {
        changes.push_back(change_action);
    }
}

ChangeAction ChangeQueue::pop()
{
    ChangeAction action = changes.back();
    changes.pop_back();
    return action;
}

void ChangeQueue::clear()
{
    changes.clear();
}