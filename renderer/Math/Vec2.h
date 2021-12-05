#pragma once

#include <assert.h>
#include <initializer_list>
#include "util.h"

namespace glacier {

template<typename T>
struct Vec2 {
    using value_type = T;
    static constexpr int kSize = 2;
    static constexpr size_t value_size = sizeof(T);
    static const Vec2<T> zero;
    static const Vec2<T> half;
    static const Vec2<T> one;

    static const Vec2<T> down;
    static const Vec2<T> up;
    static const Vec2<T> left;
    static const Vec2<T> right;

    union {
        T value[kSize];
        struct { T x, y; };
        struct { T u, v; };
    };

    constexpr Vec2() : x(0), y(0) {}

    constexpr Vec2(T x_, T y_) : x(x_), y(y_) {
    }

    constexpr explicit Vec2(T v) : x(v), y(v) {
    }

    constexpr Vec2(const Vec2<T>& v) : x(v.x), y(v.y) {
    }

    constexpr explicit Vec2(float* v) : x(v[0]), y(v[1]) {

    }

    Vec2<T>& operator =(float* v) {
        assert(v);
        x = value[0];
        y = value[1];
        return *this;
    }

    void Set(T x, T y) {
        value[0] = x;
        value[1] = y;
    }

    void Zero() {
        x = 0;
        y = 0;
    }

    Vec2<T> Abs() const {
        return Vec2<T>(
            math::Abs(x),
            math::Abs(y)
        );
    }

    void Normalize() {
        T mag = Magnitude();
        if (mag > math::kEpsilon) {
            T inv = (T)1 / mag;
            x *= inv;
            y *= inv;
        } else {
            x = 0;
            y = 1;
        }

        return *this;
    }

    Vec2<T> Normalized() const {
        T mag = Magnitude();
        T inv = (T)1 / mag;
        return Vec2<T>(x * inv, y * inv);
    }

    Vec2<T> NormalizedSafe(const Vec2<T>& default_value) const {
        T mag = Magnitude();
        if (mag > math::kEpsilon) {
            T inv = (T)1 / mag;
            return Vec2<T>(x * inv, y * inv);
        }

        return default_value;
    }

    T MagnitudeSq() const {
        return x * x + y * y;
    }

    T Magnitude() const {
        return sqrt(MagnitudeSq());
    }

    T Dot(const Vec2<T>& v) const {
        return x * v.x + y * v.y;
    }

    T Distance(const Vec2<T>& v) const {
        Vec2<T> diff = *this - v;
        return diff.Magnitude();
    }

    T DistanceSq(const Vec2<T>& v) const {
        Vec2<T> diff = *this - v;
        return diff.MagnitudeSq();
    }

    Vec2<T> Scale(T v) {
        Normalize();
        x *= v;
        y *= v;

        return *this;
    }

    void Limit(T v) {
        T mag = Magnitude();
        if (mag > v) {
            T inv = (T)1 / mag * v;
            x = x * inv;
            y = y * inv;
        }
    }

    Vec2<T> Neg() const {
        return Vec2<T>(-x, -y);
    }

    Vec2<T> Sqrt() const {
        return Vec2<T>(sqrt(x), sqrt(y));
    }

    T Cross(const Vec2<T>& v) const {
        return x * v.y - y * v.x;
    }

    bool AlmostEquals(const Vec2<T>& v, T epsilon = math::kEpsilon) const {
        return math::AlmostEqual(x, v.x, epsilon) && math::AlmostEqual(y, v.y, epsilon);
    }

    int LeastSignificantComponent() {
        T absX = math::Abs(x);
        T absY = math::Abs(y);

        if (absX < absY) {
            return 0;
        } else {
            return 1;
        }
    }

    T operator[](int index) const {
        assert(index >= 0 && index < kSize);
        return value[index];
    }

    T& operator[](int index) {
        assert(index >= 0 && index < kSize);
        return value[index];
    }

    Vec2<T>& operator =(const Vec2<T>& v) {
        x = v.x;
        y = v.y;

        return *this;
    }

    Vec2<T> operator +() const {
        return Vec2<T>(x, y);
    }

    Vec2<T> operator +(const Vec2<T>& v) const {
        return Vec2<T>(x + v.x, y + v.y);
    }

    Vec2<T> operator +(T v) const {
        return Vec2<T>(x + v, y + v);
    }

    Vec2<T>& operator +=(T v) {
        x += v;
        y += v;
        return *this;
    }

    Vec2<T>& operator +=(const Vec2<T>& v) {
        x += v.x;
        y += v.y;
        return *this;
    }

    Vec2<T> operator -(const Vec2<T>& v) const {
        return Vec2<T>(x - v.x, y - v.y);
    }

    Vec2<T> operator -(T v) const {
        return Vec2<T>(x - v, y - v);
    }

    Vec2<T> operator -() const {
        return Vec2<T>(-x, -y);
    }

    Vec2<T>& operator -=(T v) {
        x -= v;
        y -= v;
        return *this;
    }

    Vec2<T>& operator -=(const Vec2<T>& v) {
        x -= v.x;
        y -= v.y;
        return *this;
    }

    Vec2<T> operator *(const Vec2<T>& v) const {
        return Vec2<T>(x * v.x, y * v.y);
    }

    Vec2<T> operator *(T v) const {
        return Vec2<T>(x * v, y * v);
    }

    Vec2<T>& operator *=(T v) {
        x *= v;
        y *= v;
        return *this;
    }

    Vec2<T>& operator *=(const Vec2<T>& v) {
        x *= v.x;
        y *= v.y;
        return *this;
    }

    Vec2<T> operator /(const Vec2<T>& v) const {
        return Vec2<T>(x / v.x, y / v.y);
    }

    Vec2<T> operator /(T v) const {
        T inv = (T)1 / v;
        return Vec2<T>(x * inv, y * inv);
    }

    Vec2<T>& operator /=(T v) {
        T inv = (T)1 / v;
        x *= inv;
        y *= inv;
        return *this;
    }

    Vec2<T>& operator /=(const Vec2<T>& v) {
        x /= v.x;
        y /= v.y;
        return *this;
    }

    bool operator ==(const Vec2<T>& v) const {
        return x == v.x && y == v.y;
    }

    bool operator !=(const Vec2<T>& v) const {
        return x != v.x || y != v.y;
    }

    static Vec2<T> Min(const Vec2<T>& left, const Vec2<T>& right) {
        return Vec2<T>(math::Min(left.x, right.x), math::Min(left.y, right.y));
    }

    static Vec2<T> Max(const Vec2<T>& left, const Vec2<T>& right) {
        return Vec2<T>(math::Max(left.x, right.x), math::Max(left.y, right.y));
    }
    
    static Vec2<T> Lerp(const Vec2<T>& v0, const Vec2<T>& v1, float t) {
        return v0 * (1.0f - t) + v1 * t;
    }
};


template<typename T>
Vec2<T> operator *(T scalar, const Vec2<T>& rhs) {
    return Vec2<T>(rhs.x * scalar, rhs.y * scalar);
}

template<typename T>
Vec2<T> operator /(T scalar, const Vec2<T>& rhs) {
    return Vec2<T>(scalar / rhs.x, scalar / rhs.y);
}

template<typename T>
Vec2<T> operator -(T scalar, const Vec2<T>& rhs) {
    return Vec2<T>(scalar - rhs.x, scalar - rhs.y);
}

template<typename T>
Vec2<T> operator +(T scalar, const Vec2<T>& rhs) {
    return Vec2<T>(scalar + rhs.x, scalar + rhs.y);
}

template<typename T>
const Vec2<T> Vec2<T>::zero((T)0);

template<typename T>
const Vec2<T> Vec2<T>::half((T)0.5);

template<typename T>
const Vec2<T> Vec2<T>::one((T)1);

template<typename T>
const Vec2<T> Vec2<T>::down((T)0, (T)-1);

template<typename T>
const Vec2<T> Vec2<T>::up((T)0, (T)1);

template<typename T>
const Vec2<T> Vec2<T>::left((T)-1, (T)0);

template<typename T>
const Vec2<T> Vec2<T>::right((T)1, (T)0);

using Vec2f = Vec2<float>;
using Vec2i = Vec2<int>;
using Vector2 = Vec2<float>;

}

