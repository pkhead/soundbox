/**
* Numeric utilities
**/
#pragma once
#include <imgui.h>
#include <cmath>
#include <string>

// vector2 class fully compatible with ImGui's Vec2
// this is so i can do vector math easily
struct Vec2 {
    float x, y;
    
    constexpr Vec2() : x(0.0f), y(0.0f) {}
    constexpr Vec2(float _x, float _y) : x(_x), y(_y) {}
    Vec2(const ImVec2& src): x(src.x), y(src.y) {}

    Vec2 operator+(const Vec2& other) const {
        return Vec2(x + other.x, y + other.y);
    }

    Vec2 operator-(const Vec2& other) const {
        return Vec2(x - other.x, y - other.y);
    }

    Vec2 operator*(const Vec2& other) const {
        return Vec2(x * other.x, y * other.y);
    }

    Vec2 operator/(const Vec2& other) const {
        return Vec2(x / other.x, y / other.y);
    }

    bool operator==(const Vec2& other) const {
        return x == other.x && y == other.y;
    }

    bool operator!=(const Vec2& other) const {
        return x != other.x || y != other.y;
    }

    float magn_sq() const {
        return x * x + y * y; 
    }

    float magn() const {
        return sqrtf(x * x + y * y);
    }

    template <typename T>
    Vec2 operator*(const T& scalar) const {
        return Vec2(x * scalar, y * scalar);
    }

    template <typename T>
    Vec2 operator/(const T& scalar) const {
        return Vec2(x / scalar, y / scalar);
    }

    operator ImVec2() const { return ImVec2(x, y); }
};

template <typename T = float>
struct complex_t
{
    T real;
    T imag;

    inline constexpr complex_t()                : real(0.0f), imag(0.0f) {}
    inline constexpr complex_t(T real)          : real(real), imag(0.0f) {}
    inline constexpr complex_t(T real, T imag)  : real(real), imag(imag) {}

    inline constexpr complex_t operator+(const complex_t& other) {
        return complex_t(real + other.real, imag + other.imag);
    }

    inline constexpr complex_t operator+(const T& scalar) {
        return complex_t(real + scalar, imag);
    }

    inline constexpr complex_t operator-(const complex_t& other) {
        return complex_t(real - other.real, imag - other.imag);
    }

    inline constexpr complex_t operator-(const T& scalar) {
        return complex_t(real - scalar, imag);
    }

    inline constexpr complex_t operator*(const complex_t& other) {
        return complex_t(
            real * other.real - imag * other.imag,
            real * other.imag + imag * other.real
        );
    }

    inline constexpr complex_t operator/(const complex_t& other) {
        T val = other.real * other.real + other.imag * other.imag;

        return complex_t(
            (real * other.real + imag * other.imag) / val,
            (imag * other.real - real * other.imag) / val
        );
    }

    inline constexpr complex_t operator-() {
        return complex_t(-real, -imag);
    }

    inline constexpr complex_t operator==(const complex_t& other) {
        return real == other.real && imag == other.imag;
    }
};

namespace util
{
    template <typename T>
    constexpr T min(T a, T b)
    {
        return a < b ? a : b;
    }

    template <typename T>
    constexpr T max(T a, T b)
    {
        return a > b ? a : b;
    }

    template <typename T>
    constexpr T lerp(T a, T b, T t)
    {
        return (b - a) * t + a;
    }

    float modf(float a, float b);
    double mod(double a, double b);

    // binary sign -- returns only two results
    template <typename T>
    inline int bsign(T v) {
        return v >= 0 ? 1 : -1; 
    }

    template <typename T>
    int sign(T v) {
        return v == 0 ? 0 : bsign(v);
    }

    template <typename T>
    T clamp(T min, T max, T v)
    {
        if (v > max) return max;
        if (v < min) return min;
        return v;
    }

    /**
    * sprintf into std::string
    **/
    __attribute__((__format__(__printf__, 1, 2)))
    std::string format(const char *fmt, ...);

    /**
    * vsprintf into std::string
    **/
    std::string vformat(const char *fmt, va_list va);
}