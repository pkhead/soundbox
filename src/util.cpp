#include <cstdio>
#include "util.hpp"

float util::modf(float a, float b) {
    return fmodf(fmodf(a, b) + b, b);
}

double util::mod(double a, double b) {
    return fmod(fmod(a, b) + b, b);
}

std::string util::format(const char *fmt, ...)
{
    // calculate length required for vsnprintf
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(nullptr, 0, fmt, args);
    assert(len > 0);

    // create buffer for data
    std::string ret;
    ret.resize(len);
    assert(ret.data()[len] == '\0'); // i'm pretty sure resize appends the \0 at the end but maybe not??

    // write formatted string into buffer
    va_start(args, fmt);
    vsnprintf(ret.data(), ret.size() + 1, fmt, args);
    va_end(args);

    return ret;
}

std::string util::vformat(const char* fmt, va_list args)
{
    // calculate required length
    va_list argc;
    va_copy(argc, args);
    int len = vsnprintf(nullptr, 0, fmt, argc);
    assert(len > 0);
    va_end(argc);

    // create buffer for data
    std::string str;
    str.resize(len);
    assert(str.data()[len] == '\0'); // i'm pretty sure resize appends \0 at the end but maybe not??

    // write formatted string into buffer
    vsnprintf(str.data(), str.size(), fmt, args);
    return str;
}