#pragma once

#include <assert.h>
#include <initializer_list>
#include <math.h>
#include "util.h"

namespace glacier {

template<typename T>
struct Vec3 {
    using value_type = T;
    static constexpr int kSize = 3;
    static constexpr size_t value_size = sizeof(T);
    static const Vec3<T> zero;
    static const Vec3<T> half;
    static const Vec3<T> one;

    static const Vec3<T> back;
    static const Vec3<T> forward;
    static const Vec3<T> left;
    static const Vec3<T> right;
    static const Vec3<T> up;
    static const Vec3<T> down;

    union {
        T value[kSize];
        struct { T x, y, z; };
        struct { T r, g, b; };
    };

    constexpr Vec3() : x(0), y(0), z(0) {}

    constexpr Vec3(T x_, T y_, T z_) : x(x_), y(y_), z(z_) {
    }

    constexpr explicit Vec3(T v) : x(v), y(v), z(v) {
    }

    constexpr Vec3(const Vec3<T>& v) : x(v.x), y(v.y), z(v.z) {
    }

    constexpr explicit Vec3(float* v) : x(v[0]), y(v[1]), z(v[2]) {

    }

    Vec3<T>& operator =(float* v) {
        assert(v);
        x = v[0];
        y = v[1];
        z = v[2];
        return *this;
    }

    void Set(T x, T y, T z) {
        value[0] = x;
        value[1] = y;
        value[2] = z;
    }

    void Zero() {
        x = 0;
        y = 0;
        z = 0;
    }

    Vec3<T> Abs() const {
        return Vec3<T>(
            math::Abs(x),
            math::Abs(y),
            math::Abs(z)
        );
    }

    void Normalize() {
        T mag = Magnitude();
        if (mag > math::kEpsilon) {
            T inv = (T)1 / mag;
            x *= inv;
            y *= inv;
            z *= inv;
        } else {
            x = 0;
            y = 0;
            z = 1;
        }

        //return *this;
    }

    Vec3<T> Normalized() const {
        T mag = Magnitude();
        T inv = (T)1.0 / mag;
        return Vec3<T>(x * inv, y * inv, z * inv);
    }

    Vec3<T> NormalizedSafe(const Vec3<T>& default_value) const {
        T mag = Magnitude();
        if (mag > math::kEpsilon) {
            T inv = (T)1.0 / mag;
            return Vec3<T>(x * inv, y * inv, z * inv);
        }

        return default_value;
    }

    T MagnitudeSq() const {
        return x * x + y * y + z * z;
    }

    T Magnitude() const {
        return ::sqrt(MagnitudeSq());
    }

    T Dot(const Vec3<T>& v) const {
        return x * v.x + y * v.y + z * v.z;
    }

    Vec3<T> Cross(const Vec3<T>& v) const {
        return Vec3<T>(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x);
    }

    T Distance(const Vec3<T>& v) const {
        Vec3<T> diff = *this - v;
        return diff.Magnitude();
    }

    T DistanceSq(const Vec3<T>& v) const {
        Vec3<T> diff = *this - v;
        return diff.MagnitudeSq();
    }

    Vec3<T> Scale(T v) {
        Normalize();
        x *= v;
        y *= v;
        z *= v;

        return *this;
    }

    void Limit(T v) {
        T mag = Magnitude();
        if (mag > v) {
            T inv = (T)1 / mag * v;
            x = x * inv;
            y = y * inv;
            z = z * inv;
        }
    }

    Vec3<T> Inverse() const {
        return Vec3<T>((T)1 / x, (T)1 / y, (T)1 / z);
    }

    Vec3<T> InverseSafe() const {
        return Vec3<T>(math::InverseSafe(x), math::InverseSafe(y), math::InverseSafe(z));
    }

    Vec3<T> Neg() const {
        return Vec3<T>(-x, -y, -z);
    }

    Vec3<T> Sqrt() const {
        return Vec3<T>(sqrt(x), sqrt(y), sqrt(z));
    }

    bool AlmostEquals(const Vec3<T>& v, T epsilon = math::kEpsilon) const {
        return math::AlmostEqual(x, v.x, epsilon) && math::AlmostEqual(y, v.y, epsilon) && math::AlmostEqual(z, v.z, epsilon);
    }

    int LeastSignificantComponent() {
        T absX = math::Abs(x);
        T absY = math::Abs(y);
        T absZ = math::Abs(z);

        if (absX < absY) {
            if (absX < absZ) {
                return 0;
            } else {
                return 2;
            }
        } else {
            if (absY < absZ) {
                return 1;
            } else {
                return 2;
            }
        }
    }

    void SetPitchYaw(T pitch, T yaw) {
        auto cosp = ::cos(pitch);
        auto sinp = ::sin(pitch);
        auto cosy = ::cos(yaw);
        auto siny = ::sin(yaw);

        x = cosp * siny;
        y = -sinp;
        z = cosp * cosy;
    }

    T pitch() const {
        return ::asin(-y);
        //return ::atan2(-y, ::sqrt(x * x + z * z));
    }

    T yaw() const {
        return ::atan2(x, z);
    }

    T operator[](int index) const {
        assert(index >= 0 && index < kSize);
        return value[index];
    }

    T& operator[](int index) {
        assert(index >= 0 && index < kSize);
        return value[index];
    }

    Vec3<T>& operator =(const Vec3<T>& v) {
        x = v.x;
        y = v.y;
        z = v.z;

        return *this;
    }

    Vec3<T> operator +() const {
        return Vec3<T>(x, y, z);
    }

    Vec3<T> operator +(const Vec3<T>& v) const {
        return Vec3<T>(x + v.x, y + v.y, z + v.z);
    }

    Vec3<T> operator +(T v) const {
        return Vec3<T>(x + v, y + v, z + v);
    }

    Vec3<T>& operator +=(T v) {
        x += v;
        y += v;
        z += v;
        return *this;
    }

    Vec3<T>& operator +=(const Vec3<T>& v) {
        x += v.x;
        y += v.y;
        z += v.z;
        return *this;
    }

    Vec3<T> operator -(const Vec3<T>& v) const {
        return Vec3<T>(x - v.x, y - v.y, z - v.z);
    }

    Vec3<T> operator -(T v) const {
        return Vec3<T>(x - v, y - v, z - v);
    }

    Vec3<T> operator -() const {
        return Vec3<T>(-x, -y, -z);
    }

    Vec3<T>& operator -=(T v) {
        x -= v;
        y -= v;
        z -= v;
        return *this;
    }

    Vec3<T>& operator -=(const Vec3<T>& v) {
        x -= v.x;
        y -= v.y;
        z -= v.z;
        return *this;
    }

    Vec3<T> operator *(const Vec3<T>& v) const {
        return Vec3<T>(x * v.x, y * v.y, z * v.z);
    }

    Vec3<T> operator *(T v) const {
        return Vec3<T>(x * v, y * v, z * v);
    }

    Vec3<T>& operator *=(T v) {
        x *= v;
        y *= v;
        z *= v;
        return *this;
    }

    Vec3<T>& operator *=(const Vec3<T>& v) {
        x *= v.x;
        y *= v.y;
        z *= v.z;
        return *this;
    }

    Vec3<T> operator /(const Vec3<T>& v) const {
        return Vec3<T>(x / v.x, y / v.y, z / v.z);
    }

    Vec3<T> operator /(T v) const {
        T inv = (T)1 / v;
        return Vec3<T>(x * inv, y * inv, z * inv);
    }

    Vec3<T>& operator /=(T v) {
        T inv = (T)1 / v;
        x *= inv;
        y *= inv;
        z *= inv;
        return *this;
    }

    Vec3<T>& operator /=(const Vec3<T>& v) {
        x /= v.x;
        y /= v.y;
        z /= v.z;
        return *this;
    }

    bool operator ==(const Vec3<T>& v) const {
        return x == v.x && y == v.y && z == v.z;
    }

    bool operator !=(const Vec3<T>& v) const {
        return x != v.x || y != v.y || z != v.z;
    }
    
    static Vec3<T> Floor(const Vec3<T>& left) {
        return Vec3<T>(math::Floor(left.x), math::Floor(left.y), math::Floor(left.z));
    }

    static Vec3<T> Min(const Vec3<T>& left, const Vec3<T>& right) {
        return Vec3<T>(math::Min(left.x, right.x), math::Min(left.y, right.y), math::Min(left.z, right.z));
    }

    static Vec3<T> Max(const Vec3<T>& left, const Vec3<T>& right) {
        return Vec3<T>(math::Max(left.x, right.x), math::Max(left.y, right.y), math::Max(left.z, right.z));
    }

    static Vec3<T> Pow(const Vec3<T>& left, T v) {
        return Vec3<T>(math::Pow(left.x, v), math::Pow(left.y, v), math::Pow(left.z, v));
    }

    static Vec3<T> Sqrt(const Vec3<T>& left) {
        return Vec3<T>(math::Sqrt(left.x), math::Sqrt(left.y), math::Sqrt(left.z));
    }

    static Vec3<T> Lerp(const Vec3<T>& v0, const Vec3<T>& v1, float t) {
        return v0 * (1.0f - t) + v1 * t;
    }

    static Vec3<T> Reflect(const Vec3<T>& i, const Vec3<T>& n) {
        return i - 2.0f * n.Dot(i) * n;
    }

    static Vec3<T> Refract(const Vec3<T>& uv, const Vec3<T>& n, T etai_over_etat) {
        auto cos_theta = fmin(n.Dot(-uv), 1.0f);
        Vec3<T> r_out_perp =  etai_over_etat * (uv + cos_theta * n);
        Vec3<T> r_out_parallel = -sqrt(fabs(1.0f - r_out_perp.MagnitudeSq())) * n;
        return r_out_perp + r_out_parallel;
    }

    // Returns the angle in degrees between /from/ and /to/. This is always the smallest
    static T Angle(const Vec3<T>& from, const Vec3<T>& to) {
        T denominator = (T)math::Sqrt(from.MagnitudeSq() * to.MagnitudeSq());
        if (denominator < math::kEpsilonSq)
            return 0.0f;

        T dot = math::Clamp(from.Dot(to) / denominator, -1.0f, 1.0f);
        return ((T)math::ACos(dot)) * math::kRad2Deg;
    }

    // Returns the angle in degrees between /from/ and /to/. This is always the smallest
    static T SignedAngle(const Vec3<T>& from, const Vec3<T>& to, const Vec3<T>& axis) {
        T unsignedAngle = Angle(from, to);

        T cross_x = from.y * to.z - from.z * to.y;
        T cross_y = from.z * to.x - from.x * to.z;
        T cross_z = from.x * to.y - from.y * to.x;
        T sign = math::Sign(axis.x * cross_x + axis.y * cross_y + axis.z * cross_z);
        return unsignedAngle * sign;
    }

    /// returns the smallest angle between ABC. Never greater than 180
    static T MinAngle(const Vec3<T>& a, const Vec3<T>& b, const Vec3<T>& c) {
        return Vec3<T>::Angle((a - b), (c - b));
    }
};

template<typename T>
Vec3<T> operator *(T scalar, const Vec3<T>& rhs) {
    return Vec3<T>(rhs.x * scalar, rhs.y * scalar, rhs.z * scalar);
}

template<typename T>
Vec3<T> operator /(T scalar, const Vec3<T>& rhs) {
    return Vec3<T>(scalar / rhs.x, scalar / rhs.y, scalar / rhs.z);
}

template<typename T>
Vec3<T> operator -(T scalar, const Vec3<T>& rhs) {
    return Vec3<T>(scalar - rhs.x, scalar - rhs.y, scalar - rhs.z);
}

template<typename T>
Vec3<T> operator +(T scalar, const Vec3<T>& rhs) {
    return Vec3<T>(scalar + rhs.x, scalar + rhs.y, scalar + rhs.z);
}

template<typename T>
const Vec3<T> Vec3<T>::zero((T)0);

template<typename T>
const Vec3<T> Vec3<T>::half((T)0.5);

template<typename T>
const Vec3<T> Vec3<T>::one((T)1);

template<typename T>
const Vec3<T> Vec3<T>::back((T)0, (T)0, (T)-1);

template<typename T>
const Vec3<T> Vec3<T>::forward((T)0, (T)0, (T)1);

template<typename T>
const Vec3<T> Vec3<T>::left((T)-1, (T)0, (T)0);

template<typename T>
const Vec3<T> Vec3<T>::right((T)1, (T)0, (T)0);

template<typename T>
const Vec3<T> Vec3<T>::up((T)0, (T)1, (T)0);

template<typename T>
const Vec3<T> Vec3<T>::down((T)0, (T)-1, (T)0);

using Vec3f = Vec3<float>;
using Vec3i = Vec3<int>;
using Vector3 = Vec3<float>;

//using Colorf = Vec3<float>;
//using Color = Vec3<uint32_t>;

}
