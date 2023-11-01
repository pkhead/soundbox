#pragma once
#include <cstddef>
#include <cstdint>
#include <atomic>
#include <thread>
#include "util.h"

#if !(defined(ATOMIC_BOOL_LOCK_FREE) | defined (__GCC_ATOMIC_BOOL_LOCK_FREE) | defined(__CLANG_ATOMIC_BOOL_LOCK_FREE))
#error std::atomic<bool> is not lock free
#endif

typedef void (*WorkProcedure)(void* userdata, size_t data_size);

class WorkScheduler
{
public:

    static constexpr size_t DATA_CAPACITY = 128;
    static constexpr size_t QUEUE_CAPACITY = 8;

    WorkScheduler();
    
    /**
    * Schedule a procedure to be called in a non-realtime thread
    * It will copy the provided userdata.
    * @param userdata The data to copy
    * @param size The size of `userdata`
    * @returns True on success, false if userdata is too large
    **/
    bool schedule(WorkProcedure proc, void* userdata, size_t size);
    void run();

private:
    struct ScheduleCall {
        WorkProcedure proc;
        size_t data_size;        
        uint8_t data[DATA_CAPACITY];
    };

    MessageQueue schedule_queue;
};