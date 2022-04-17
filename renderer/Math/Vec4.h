#pragma once

#include <assert.h>
#include <stdint.h>
#include "util.h"
#include "vec3.h"

namespace glacier {

template<typename T>
struct Vec4 {
    using value_type = T;

    static constexpr int kSize = 4;
    static constexpr size_t value_size = sizeof(T);
    static const Vec4<T> zero;
    static const Vec4<T> half;
    static const Vec4<T> one;

    union {
        T value[kSize];
        struct { T x, y, z, w; };
        struct { T r, g, b, a; };
    };

    constexpr Vec4() : x(0), y(0), z(0), w(0) {}

    constexpr Vec4(T x_, T y_, T z_, T w_) : x(x_), y(y_), z(z_), w(w_) {
    }

    constexpr explicit Vec4(T v) : x(v), y(v), z(v), w(v) {
    }

    constexpr Vec4(const Vec4<T>& v) : x(v.x), y(v.y), z(v.z), w(v.w) {
    }

    constexpr explicit Vec4(float* v) : x(v[0]), y(v[1]), z(v[2]), w(v[3]) {

    }

    constexpr Vec4(const Vec3<T>& v, T w_=(T)0) : x(v.x), y(v.y), z(v.z), w(w_) {

    }

    Vec4<T>& operator =(float* v) {
        assert(v);
        x = value[0];
        y = value[1];
        z = value[2];
        w = value[3];
        return *this;
    }

    void Set(T x, T y, T z, T w) {
        value[0] = x;
        value[1] = y;
        value[2] = z;
        value[3] = w;
    }

    void Zero() {
        x = 0;
        y = 0;
        z = 0;
        w = 0;
    }

    Vec4<T>& Abs() {
        x = math::Abs(x);
        y = math::Abs(y);
        z = math::Abs(z);
        w = math::Abs(w);
        return *this;
    }

    void Normalize() {
        //TODO: check zero div
        T mag = Magnitude();
        T inv = (T)1.0f / mag;
        x *= inv;
        y *= inv;
        z *= inv;
        w *= inv;
    }

    Vec4<T> Normalized() const {
        T mag = Magnitude();
        T inv = (T)1.0f / mag;
        return Vec4<T>(x * inv, y * inv, z * inv, w * inv);
    }

    T MagnitudeSq() const {
        return x * x + y * y + z * z + w * w;
    }

    T Magnitude() const {
        return math::Sqrt(MagnitudeSq());
    }

    T Dot(const Vec4<T>& v) const {
        return x * v.x + y * v.y + z * v.z + w * v.w;
    }

    void Scale(T v) {
        Normalize();
        x *= v;
        y *= v;
        z *= v;
        w *= v;
    }

    void Limit(T v) {
        T mag = Magnitude();
        if (mag > v) {
            T inv = (T)1.0f / mag * v;
            x = x * inv;
            y = y * inv;
            z = z * inv;
            w = w * inv;
        }
    }

    bool AlmostEquals(const Vec4<T>& v, T epsilon = math::kEpsilon) const {
        return math::AlmostEqual(x, v.x, epsilon) && math::AlmostEqual(y, v.y, epsilon) &&
            math::AlmostEqual(z, v.z, epsilon) && math::AlmostEqual(w, v.w, epsilon);
    }

    T* operator&() {
        return value;
    }

    const T* operator&() const {
        return value;
    }

    T operator[](int index) const {
        return value[index];
    }

    T& operator[](int index) {
        return value[index];
    }

    Vec4<T>& operator =(const Vec4<T>& v) {
        x = v.x;
        y = v.y;
        z = v.z;
        w = v.w;

        return *this;
    }

    Vec4<T> operator +() const {
        return Vec4<T>(x, y, z, w);
    }

    Vec4<T> operator +(const Vec4<T>& v) const {
        return Vec4<T>(x + v.x, y + v.y, z + v.z, w + v.w);
    }

    Vec4<T> operator +(T v) const {
        return Vec4<T>(x + v, y + v, z + v, w + v);
    }

    Vec4<T>& operator +=(T v) {
        x += v;
        y += v;
        z += v;
        w += v;
        return *this;
    }

    Vec4<T>& operator +=(const Vec4<T>& v) {
        x += v.x;
        y += v.y;
        z += v.z;
        w += v.w;
        return *this;
    }

    Vec4<T> operator -(const Vec4<T>& v) const {
        return Vec4<T>(x - v.x, y - v.y, z - v.z, w - v.w);
    }

    Vec4<T> operator -(T v) const {
        return Vec4<T>(x - v, y - v, z - v, w - v);
    }

    Vec4<T> operator -() const {
        return Vec4<T>(-x, -y, -z, -w);
    }

    Vec4<T>& operator -=(T v) {
        x -= v;
        y -= v;
        z -= v;
        w -= v;
        return *this;
    }

    Vec4<T>& operator -=(const Vec4<T>& v) {
        x -= v.x;
        y -= v.y;
        z -= v.z;
        w -= v.w;
        return *this;
    }

    Vec4<T> operator *(const Vec4<T>& v) const {
        return Vec4<T>(x * v.x, y * v.y, z * v.z, w * v.w);
    }

    Vec4<T> operator *(T v) const {
        return Vec4<T>(x * v, y * v, z * v, w * v);
    }

    Vec4<T>& operator *=(T v) {
        x *= v;
        y *= v;
        z *= v;
        w *= v;
        return *this;
    }

    Vec4<T>& operator *=(const Vec4<T>& v) {
        x *= v.x;
        y *= v.y;
        z *= v.z;
        w *= v.w;
        return *this;
    }

    Vec4<T> operator /(const Vec4<T>& v) const {
        return Vec4<T>(x / v.x, y / v.y, z / v.z, w / v.w);
    }

    Vec4<T> operator /(T v) const {
        T inv = (T)1.0f / v;
        return Vec4<T>(x * inv, y * inv, z * inv, w * inv);
    }

    Vec4<T>& operator /=(T v) {
        T inv = (T)1.0f / v;
        x *= inv;
        y *= inv;
        z *= inv;
        w *= inv;
        return *this;
    }

    Vec4<T>& operator /=(const Vec4<T>& v) {
        x /= v.x;
        y /= v.y;
        z /= v.z;
        w /= v.w;
        return *this;
    }

    bool operator ==(const Vec4<T>& v) const {
        return x == v.x && y == v.y && z == v.z && w == v.w;
    }

    bool operator !=(const Vec4<T>& v) const {
        return x != v.x || y != v.y || z != v.z || w != v.w;
    }

    static Vec4<T> Min(const Vec4<T>& left, const Vec4<T>& right) {
        return Vec4<T>(math::Min(left.x, right.x), math::Min(left.y, right.y), math::Min(left.z, right.z), math::Min(left.w, right.w));
    }

    static Vec4<T> Max(const Vec4<T>& left, const Vec4<T>& right) {
        return Vec4<T>(math::Max(left.x, right.x), math::Max(left.y, right.y), math::Max(left.z, right.z), math::Max(left.w, right.w));
    }
    
    static Vec4<T> Lerp(const Vec4<T>& v0, const Vec4<T>& v1, float t) {
        return v0 * (1.0f - t) + v1 * t;
    }
};

template<typename T>
Vec4<T> operator *(T scalar, const Vec4<T>& rhs) {
    return Vec4<T>(rhs.x * scalar, rhs.y * scalar, rhs.z * scalar, rhs.w * scalar);
}

template<typename T>
Vec4<T> operator /(T scalar, const Vec4<T>& rhs) {
    return Vec4<T>(scalar / rhs.x, scalar / rhs.y, scalar / rhs.z, scalar / rhs.w);
}

template<typename T>
Vec4<T> operator -(T scalar, const Vec4<T>& rhs) {
    return Vec4<T>(scalar - rhs.x, scalar - rhs.y, scalar - rhs.z, scalar - rhs.w);
}

template<typename T>
Vec4<T> operator +(T scalar, const Vec4<T>& rhs) {
    return Vec4<T>(scalar + rhs.x, scalar + rhs.y, scalar + rhs.z, scalar + rhs.w);
}

template<typename T>
const Vec4<T> Vec4<T>::zero((T)0);

template<typename T>
const Vec4<T> Vec4<T>::half((T)0.5);

template<typename T>
const Vec4<T> Vec4<T>::one((T)1);

using Vec4f = Vec4<float>;

}
