#pragma once
#include <string>

namespace logger
{
#ifdef DEBUG
    void log_debug(const char* fmt...);
#else
    inline void log_debug(const char *fmt...) {}
#endif
    void log_info(const char* fmt...);
    void log_warning(const char* fmt...);
    void log_error(const char* fmt...);
    void log_fatal(const char* fmt...);
}