#include <cstdarg>
#include <cstdio>
#include "log.hpp"

#define WRITE_MSG(fmt) \
    va_list args; \
    va_start(args, fmt); \
    vfprintf(stderr, fmt, args); \
    fprintf(stderr, "\n"); \
    va_end(args);

void logger::log_debug(const char* fmt, ...)
{
    fprintf(stderr, "[DBG] ");
    WRITE_MSG(fmt);
}

void logger::log_info(const char* fmt, ...)
{
    fprintf(stderr, "[INF] ");
    WRITE_MSG(fmt);
}

void logger::log_warning(const char* fmt, ...)
{
    fprintf(stderr, "[WRN] ");
    WRITE_MSG(fmt);
}

void logger::log_error(const char* fmt, ...)
{
    fprintf(stderr, "[ERR] ");
    WRITE_MSG(fmt);
}

void logger::log_fatal(const char* fmt, ...)
{
    fprintf(stderr, "[FTL] ");
    WRITE_MSG(fmt);
}