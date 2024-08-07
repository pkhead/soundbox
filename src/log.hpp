#pragma once
#include <string>

namespace logger
{
    void log_debug(const char* fmt...);
    void log_info(const char* fmt...);
    void log_warning(const char* fmt...);
    void log_error(const char* fmt...);
    void log_fatal(const char* fmt...);
}