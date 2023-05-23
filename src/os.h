#pragma once

#ifdef _WIN32
// Windows
#include <windows.h>

#define sleep(time) Sleep((time) * 1000)

// prevent collision with user-defined max and min function
#undef max
#undef min

#else
// Unix
#include <unistd.h>

#define sleep(time) usleep((time) * 1000000)

#endif