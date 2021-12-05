#pragma once

#include <cmath>
#include <limits>
#include <cassert>

namespace glacier {
namespace math {

//constexpr float kEpsilon = std::numeric_limits<float>::epsilon();
constexpr float kEpsilon = 0.00001f;
constexpr float kEpsilonSq = kEpsilon * kEpsilon;
constexpr float kPI = 3.14159265358979323f;
constexpr float k2PI = kPI * 2.0f;
constexpr float kInvPI = 1.0f / kPI;
constexpr float kInv2PI = 1.0f / k2PI;
constexpr float kDeg2Rad = kPI / 180.0f;
constexpr float kRad2Deg = 180.0f / kPI;
constexpr float kInfinite = std::numeric_limits<float>::infinity();

template <typename T>
inline constexpr bool IsPowOf2(T x) {
    return (x & (x - 1)) == 0;
}

template<typename T>
T Sign(T v) {
    if (v >= 0) return 1;
    return -1;
}

template<typename T>
T Abs(T v) {
    if (v < 0) v = -v;
    return v;
}

template<typename T>
T Max(T a, T b) {
    return a > b ? a : b;
}

template<typename T>
T Min(T a, T b) {
    return a > b ? b : a;
}

template<typename T>
T InverseSafe(T v) {
    if (Abs(v) > kEpsilon) {
        return 1.0f / v;
    } else {
        return 0;
    }
}

template<typename T>
T Clamp(T v, T min, T max) {
    if (v < min) return min;
    else if (v > max) return max;
    return v;
}

inline float Clamp(float v, float min, float max) {
    return Clamp<float>(v, min, max);
}

inline float Saturate(float v) {
    return Clamp<float>(v, 0.0f, 1.0f);
}

inline double Saturate(double v) {
    return Clamp<double>(v, 0.0, 1.0);
}

inline int Round(float v) {
    if (v > 0) {
        return (int)(v + 0.5f);
    }
    else {
        return (int)(v - 0.5f);
    }
}

inline int64_t Round(double v) {
    if (v > 0) {
        return (int64_t)(v + 0.5f);
    }
    else {
        return (int64_t)(v - 0.5f);
    }
}

template<typename T>
inline T Floor(T v) {
    return ::floor(v);
}

template<typename T>
inline T Ceil(T v) {
    return ::ceil(v);
}


template<typename T>
inline T Fmod(T x, T y) {
    return ::fmod(x, y);
}

inline float Repeat(float t, float length)
{
    return Clamp(t - Floor(t / length) * length, 0.0f, length);
}

inline float Lerp(float a, float b, float t) {
    return a * (1.0f - t) + b * t;
}

inline double Lerp(double a, double b, double t) {
    return a * (1.0 - t) + b * t;
}

inline float InverseLerp(float a, float b, float value) {
    if (a != b)
        return Saturate((value - a) / (b - a));
    else
        return 0.0f;
}

inline float LerpAngle(float a, float b, float t) {
    float delta = Repeat((b - a), 360.0f);
    if (delta > 180.0f)
        delta -= 360.0f;
    return a + delta * Saturate(t);
}

inline float DeltaAngle(float current, float target) {
    float delta = Repeat((target - current), 360.0f);
    if (delta > 180.0f)
        delta -= 360.0f;
    return delta;
}

inline bool AlmostEqual(float left, float right, float epsilon = kEpsilon) {
    return Abs(left - right) < epsilon;
}

inline bool AlmostEqual(double left, double right, double epsilon = kEpsilon) {
    return Abs(left - right) < epsilon;
}

inline float Pow(float a, float b) {
    return ::pow(a, b);
}

inline double Pow(double a, double b) {
    return ::pow(a, b);
}

inline float Sqrt(float a) {
    return ::sqrt(a);
}

inline double Sqrt(double a) {
    return ::sqrt(a);
}

inline float Sin(float a) {
    return ::sin(a);
}

inline float ASin(float a) {
    return ::asin(a);
}

inline float Cos(float a) {
    return ::cos(a);
}

inline float ACos(float a) {
    return ::acos(a);
}

inline float Tan(float a) {
    return ::cos(a);
}

inline float ATan(float a) {
    return ::atan(a);
}

inline float ATan2(float y, float x) {
    return ::atan2(y, x);
}

template<typename T>
T WrapAngle(T theta) noexcept
{
    constexpr T k2PI = (T)2.0 * (T)kPI;
    const T mod = (T)::fmod(theta, k2PI);
    if (mod > (T)kPI) {
        return mod - k2PI;
    } else if (mod < -(T)kPI) {
        return mod + k2PI;
    }
    return mod;
}

inline void FastSinCos(float* pSin, float* pCos, float value) {
    assert(pSin);
    assert(pCos);

    // Map Value to y in [-pi,pi], x = 2*pi*quotient + remainder.
    float quotient = 1.0f / (2 * kPI) * value;
    if (value >= 0.0f) {
        quotient = static_cast<float>(static_cast<int>(quotient + 0.5f));
    } else {
        quotient = static_cast<float>(static_cast<int>(quotient - 0.5f));
    }
    float y = value - 2 * kPI * quotient;

    // Map y to [-pi/2,pi/2] with sin(y) = sin(Value).
    float sign;
    if (y > kPI / 2.0f) {
        y = kPI - y;
        sign = -1.0f;
    } else if (y < -kPI / 2.0f) {
        y = -kPI - y;
        sign = -1.0f;
    } else {
        sign = +1.0f;
    }

    float y2 = y * y;

    // 11-degree minimax approximation
    *pSin = (((((-2.3889859e-08f * y2 + 2.7525562e-06f) * y2 - 0.00019840874f) * y2 + 0.0083333310f) * y2 - 0.16666667f) * y2 + 1.0f) * y;

    // 10-degree minimax approximation
    float p = ((((-2.6051615e-07f * y2 + 2.4760495e-05f) * y2 - 0.0013888378f) * y2 + 0.041666638f) * y2 - 0.5f) * y2 + 1.0f;
    *pCos = sign * p;
}


/***************************************************************************
* These functions were taken from the MiniEngine.
* Source code available here:
* https://github.com/Microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/Math/Common.h
* Retrieved: January 13, 2016
**************************************************************************/
template <typename T>
inline T AlignUpWithMask(T value, size_t mask)
{
    return (T)(((size_t)value + mask) & ~mask);
}

template <typename T>
inline T AlignDownWithMask(T value, size_t mask)
{
    return (T)((size_t)value & ~mask);
}

template <typename T>
inline T AlignUp(T value, size_t alignment)
{
    return AlignUpWithMask(value, alignment - 1);
}

template <typename T>
inline T AlignDown(T value, size_t alignment)
{
    return AlignDownWithMask(value, alignment - 1);
}

template <typename T>
inline bool IsAligned(T value, size_t alignment)
{
    return 0 == ((size_t)value & (alignment - 1));
}

template <typename T>
inline T DivideByMultiple(T value, size_t alignment)
{
    return (T)((value + alignment - 1) / alignment);
}
/***************************************************************************/


}
}
