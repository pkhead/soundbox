#include "change_history.h"
#include <cstdlib>
#include <cstring>

ValueChangeAction::ValueChangeAction(void* address, void* data, size_t size) : address(address), size(size)
{
    this->data = malloc(size);
    memcpy(this->data, data, size);
    change_type = ChangeActionType::Value;
}

ValueChangeAction::~ValueChangeAction()
{
    free(data);
}

void ValueChangeAction::set()
{
    memcpy(address, data, size);
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

ChangeAction* ChangeQueue::pop()
{
    if (changes.empty()) return nullptr;
    
    ChangeAction* change = changes.back();
    change->set();
    changes.pop_back();

    return change;
}

void ChangeQueue::finalize_pop()
{
    if (cur_change != nullptr)
    {
        delete cur_change;
        cur_change = nullptr;
        changes.pop_back();
    }
}

void ChangeQueue::clear()
{
    for (ChangeAction* change : changes)
        delete change;

    changes.clear();
}