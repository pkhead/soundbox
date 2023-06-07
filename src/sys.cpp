#include <ctime>
#include "sys.h"

const double elapsed(const clock_t& since)
{
    clock_t ticks_elapsed = clock() - since;
    return (double)ticks_elapsed / CLOCKS_PER_SEC;
}