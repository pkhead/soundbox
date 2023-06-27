#include <cstring>
#include "worker.h"

bool WorkScheduler::schedule(WorkProcedure proc, void* userdata, size_t size)
{
    if (size >= DATA_CAPACITY || num_queued >= QUEUE_CAPACITY)
        return false;
    
    ScheduleCall& slot = schedule_queue[num_queued++];

    slot.proc = proc;

    if (userdata == nullptr)
        slot.data_size = 0;
    else {
        slot.data_size = size; 
        memcpy(slot.data, userdata, size);
    }

    return true;
}

void WorkScheduler::run()
{
    for (int i = 0; i < num_queued; i++)
    {
        ScheduleCall& slot = schedule_queue[i];
        slot.proc(slot.data, slot.data_size);
    }

    num_queued = 0;
}