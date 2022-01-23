#pragma once

#include <cstdint>
#include "rand.h"
#include "vec2.h"
#include "vec3.h"
#include "quat.h"
#include "util.h"

namespace glacier {
namespace random {

extern Rand g_rng;

inline void SetSeed(uint32_t sd) {
    g_rng.SetSeed(sd);
}

inline uint32_t Value() {
    return g_rng.Get();
}

template<typename T>
inline T Value() {
    return (T)g_rng.GetFloat();
}

template<typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
T Range(T min, T max) {
    int dif;
    if (min < max) {
        dif = max - min + 1;
        int t = g_rng.Get() % dif;
        t += min;
        return t;
    } else if (min > max) {
        dif = min - max + 1;
        int t = g_rng.Get() % dif;
        t = min - t;
        return t;
    } else {
        return min;
    }        
}

template<typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
T Range(T min, T max) {
    T t = Value<T>();
    t = min * t + (1.0f - t) * max;
    return t;
}

inline Vec3f Vector() {
    return Vec3f(Value<float>(), Value<float>(), Value<float>());
}

inline Vec3f Vector(float min, float max) {
    return Vec3f(Range<float>(min, max), Range<float>(min, max), Range<float>(min, max));
}

inline Vec3f UnitVector() {
    float z = Range<float>(-1.0f, 1.0f);
    float a = Range<float>(0.0f, 2.0f * math::kPI);

    float r = ::sqrt (1.0f - z*z);

    float x = r * ::cos (a);
    float y = r * ::sin (a);

    return Vec3f(x, y, z);
}

inline Vec2f UnitVec2f() {
    float a = Range<float>(0.0f, 2.0f * math::kPI);

    float x = ::cos(a);
    float y = ::sin(a);

    return Vec2f(x, y);
}

inline auto Quaternion() {
    glacier::Quaternion q;
    q.x = Range<float>(-1.0, 1.0);
    q.y = Range<float>(-1.0, 1.0);
    q.z = Range<float>(-1.0, 1.0);
    q.w = Range<float>(-1.0, 1.0);
    q.Normalize();
    if (q.Dot(Quaternion::identity) < 0.0)
        return -q;
    else
        return q;
}

inline Vec3f PointInsideUnitSphere() {
    Vec3f v = UnitVector();
    v *= math::Pow(Value<float>(), (float)(1.0 / 3.0));
    return v;
}

inline Vec2f PointInsideUnitCircle() {
    Vec2f v = UnitVec2f();
    // As the volume of the sphere increases (x^3) over an interval we have to increase range as well with x^(1/3)
    v *= math::Pow(Value<float>(), (float)(1.0 / 2.0));
    return v;
}

}
}