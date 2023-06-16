#include "change_history.h"
#include <cstdlib>
#include <cstring>

ValueChangeAction::ValueChangeAction(void* address, void* old_data, void* new_data, size_t size) : address(address), size(size)
{
    this->old_data = malloc(size);
    this->new_data = malloc(size);

    memcpy(this->old_data, old_data, size);
    memcpy(this->new_data, new_data, size);

    change_type = ChangeActionType::Value;
}

ValueChangeAction::~ValueChangeAction()
{
    free(old_data);
    free(new_data);
}

void ValueChangeAction::undo()
{
    memcpy(address, old_data, size);
}

void ValueChangeAction::redo()
{
    memcpy(address, new_data, size);
}

bool ValueChangeAction::can_merge(ChangeAction* other)
{
    if (other->change_type != change_type) return false;
    ValueChangeAction* other_subclass = (ValueChangeAction*) other;

    return address == other_subclass->address;
}

ChangeQueue::~ChangeQueue()
{
    clear();
}

void ChangeQueue::push(ChangeAction* change_action)
{
    // don't register change if mergeable
    if (!changes.empty() && change_action->can_merge(changes.back()))
    {
        delete change_action;
        return;
    }
    else
    {
        changes.push_back(change_action);
    }
}

ChangeAction* ChangeQueue::undo()
{
    if (changes.empty()) return nullptr;
    
    ChangeAction* change = changes.back();
    change->undo();
    changes.pop_back();

    return change;
}

ChangeAction* ChangeQueue::redo()
{
    if (changes.empty()) return nullptr;

    ChangeAction* change = changes.back();
    change->redo();
    changes.pop_back();

    return change;
}

void ChangeQueue::clear()
{
    for (ChangeAction* change : changes)
        delete change;

    changes.clear();
}