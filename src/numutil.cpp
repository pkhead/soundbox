#include <numutil.hpp>

float util::modf(float a, float b) {
    return fmodf(fmodf(a, b) + b, b);
}

double util::mod(double a, double b) {
    return fmod(fmod(a, b) + b, b);
}