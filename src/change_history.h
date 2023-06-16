#include <cstdint>
#include <imgui.h>
#include <vector>

enum class ChangeActionType : uint8_t
{
    Unknown = 0,
    Value
};

class ChangeAction
{
public:
    virtual ~ChangeAction() {};
    ChangeActionType change_type = ChangeActionType::Unknown;

    virtual void set() = 0;
    virtual bool can_merge(ChangeAction* other) = 0;
};

class ValueChangeAction : public ChangeAction
{
public:
    ValueChangeAction(void* address, void* data, size_t size);
    ~ValueChangeAction();

    void* address;
    void* data;
    size_t size;

    void set() override;
    bool can_merge(ChangeAction* other) override;
};

class ChangeQueue
{
private:
    std::vector<ChangeAction*> changes;
public:
    ~ChangeQueue();
    
    ChangeAction* cur_change = nullptr;

    void push(ChangeAction* action);
    ChangeAction* pop();
    void finalize_pop();
    void clear();
};