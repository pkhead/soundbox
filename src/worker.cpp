#include <cstring>
#include <cassert>
#include "worker.h"

WorkScheduler::WorkScheduler()
:   schedule_queue(DATA_CAPACITY, QUEUE_CAPACITY)
{}

bool WorkScheduler::schedule(WorkProcedure proc, void* userdata, size_t size)
{
    ScheduleCall call;
    call.proc = proc;
    if (userdata == nullptr)
        call.data_size = 0;
    else
    {
        call.data_size = size;
        memcpy(call.data, userdata, size);
    }   

    return schedule_queue.post(&call, sizeof(call));
}

void WorkScheduler::run()
{
    ScheduleCall call_slot;

    while (true)
    {
        auto handle = schedule_queue.read();
        if (!handle) break;
        assert(handle.size() == sizeof(ScheduleCall));

        handle.read(&call_slot, sizeof(ScheduleCall));
        call_slot.proc(call_slot.data, call_slot.data_size);
    }
}