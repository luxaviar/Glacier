#pragma once

#include "util.h"
#include "vec3.h"

namespace glacier {

template<typename T>
struct Mat3x3 {
    using value_type = T;
    static constexpr int rows = 3;
    static constexpr int cols = 3;

    static const Mat3x3<T> identity;
    static const Mat3x3<T> zero;

    union {
        T m[rows][cols];
        struct { Vec3<T> r[rows]; };
        struct { Vec3<T> r0, r1, r2; };
    };

    constexpr Mat3x3() {}

    constexpr Mat3x3(T m00, T m01, T m02,
                      T m10, T m11, T m12,
                      T m20, T m21, T m22)
        : m{m00, m01, m02,
            m10, m11, m12,
            m20, m21, m22} {
    }

    constexpr Mat3x3(const Vec3<T>& r0_, const Vec3<T>& r1_, const Vec3<T>& r2_)
        : r0(r0_), r1(r1_), r2(r2_) {
    }

    constexpr Mat3x3(const Mat3x3<T>& m) 
        : r0(m.r0), r1(m.r1), r2(m.r2) {
    }

    void Zero() {
        r0.Zero();
        r1.Zero();
        r2.Zero();
    }

    Vec3<T> ToEuler() const {
        Vec3<T> euler;
        if (m[1][2] < 0.999f) {
            if (m[1][2] > -0.999f) {
                euler.x = math::ASin(-m[1][2]);
                euler.y = math::ATan2(m[0][2], m[2][2]);
                euler.z = math::ATan2(m[1][0], m[1][1]);
            }
            else {
                // WARNING.  Not unique.  YA - ZA = atan2(r01,r00)
                euler.x = math::kPI * 0.5f;
                euler.y = math::ATan2(m[0][1], m[0][0]);
                euler.z = 0.0f;
            }
        } else {
            // WARNING.  Not unique.  YA + ZA = atan2(-r01,r00)
            euler.x = -math::kPI * 0.5f;
            euler.y = math::ATan2(m[0][1], m[0][0]);
            euler.z = 0.0f;
        }

        const float negativeFlip = -0.0001f;
        const float positiveFlip = math::kPI * 2.0f - 0.0001f;

        if (euler.x < negativeFlip)
            euler.x += 2.0f * math::kPI;
        else if (euler.x > positiveFlip)
            euler.x -= 2.0f * math::kPI;

        if (euler.y < negativeFlip)
            euler.y += 2.0f * math::kPI;
        else if (euler.y > positiveFlip)
            euler.y -= 2.0f * math::kPI;

        if (euler.z < negativeFlip)
            euler.z += 2.0f * math::kPI;
        else if (euler.z > positiveFlip)
            euler.z -= 2.0f * math::kPI;

        return euler;
    }

    Mat3x3<T> Transpose() const {
        return Mat3x3<T>(
            r0.x, r1.x, r2.x,
            r0.y, r1.y, r2.y,
            r0.z, r1.z, r2.z
        );
    }

    T Determinant() const {
        return (r0.x * (r1.y * r2.z - r2.y * r1.z) -
            r0.y * (r1.x * r2.z - r2.x * r1.z) +
            r0.z * (r1.x * r2.y - r2.x * r1.y));
    }

    bool Inverse() {
        T determinant = Determinant();
        if (determinant < math::kEpsilon) {
            return false;
        }

        T inv_det = 1.0f / determinant;

        Mat3x3<T> tmp((r1.y * r2.z - r2.y * r1.z), -(r0.y * r2.z - r2.y * r0.z), (r0.y * r1.z - r0.z * r1.y),
                    -(r1.x * r2.z - r2.x * r1.z), (r0.x * r2.z - r2.x * r0.z), -(r0.x * r1.z - r1.x * r0.z),
                    (r1.x * r2.y - r2.x * r1.y), -(r0.x * r2.y - r2.x * r0.y), (r0.x * r1.y - r0.y * r1.x));

        tmp *= inv_det;

        *this = tmp;
        return true;
    }

    T operator()(int row, int col) const {
        return m[row][col];
    }

    T& operator()(int row, int col) {
        return m[row][col];
    }

    const Vec3<T>& operator[](int index) const {
        return r[index];
    }

    Vec3<T>& operator[](int index) {
        return r[index];
    }

    Mat3x3<T>& operator =(const Mat3x3<T>& v) {
        r0 = v.r0;
        r1 = v.r1;
        r2 = v.r2;

        return *this;
    }

    Mat3x3<T>& operator -=(const Mat3x3<T>& v) {
        r0 -= v.r0;
        r1 -= v.r1;
        r2 -= v.r2;
        return *this;
    }

    Mat3x3<T>& operator +=(const Mat3x3<T>& v) {
        r0 += v.r0;
        r1 += v.r1;
        r2 += v.r2;
        return *this;
    }
    
    Mat3x3<T> operator +(const Mat3x3<T>& v) {
        return Mat3x3<T>(
            r0 + v.r0,
            r1 + v.r1,
            r2 + v.r2
        );
    }

    Mat3x3<T>& operator *=(T v) {
        r0 *= v;
        r1 *= v;
        r2 *= v;
        return *this;
    }

    Mat3x3<T>& operator *=(const Mat3x3<T>& v) {
        r0 = v.r0 * r0.x + v.r1 * r0.y + v.r2 * r0.z;
        r1 = v.r0 * r1.x + v.r1 * r1.y + v.r2 * r1.z;
        r2 = v.r0 * r2.x + v.r1 * r2.y + v.r2 * r2.z;
        return *this;
    }

    Mat3x3<T> operator *(const Mat3x3<T>& v) const {
        return Mat3x3<T>(
            v.r0 * r0.x + v.r1 * r0.y + v.r2 * r0.z,
            v.r0 * r1.x + v.r1 * r1.y + v.r2 * r1.z,
            v.r0 * r2.x + v.r1 * r2.y + v.r2 * r2.z
        );
    }

    Vec3<T> operator *(const Vec3<T>& v) const {
        return Vec3<T>(
            r0.x * v.x + r0.y * v.y + r0.z * v.z,
            r1.x * v.x + r1.y * v.y + r1.z * v.z,
            r2.x * v.x + r2.y * v.y + r2.z * v.z
        );
    }

    Mat3x3<T> operator *(T v) const {
        return Mat3x3<T>(
            r0 * v,
            r1 * v,
            r2 * v
        );
    }

    static Mat3x3<T> LookRotation(const Vec3<T>& forward, const Vec3<T>& upward) {
        T mag = forward.Magnitude();
        if (mag < math::kEpsilon) {
            return identity;
        }

        Vec3<T> z = forward / mag;
        Vec3<T> x = upward.Cross(z);

        mag = x.Magnitude();
        if (mag < math::kEpsilon) {
            return identity;
        }

        x = x / mag;
        Vec3<T> y = z.Cross(x);
        if (!math::AlmostEqual(y.MagnitudeSq(), 1.0f)) {
            return identity;
        }

        return Mat3x3<T>(
            x.x, y.x, z.x,
            x.y, y.y, z.y,
            x.z, y.z, z.z
            );
    }

    static Mat3x3<T> FromDiagonal(Vec3<T> diagnal) {
        return Mat3x3<T>(
            diagnal.x, 0.0f, 0.0f,
            0.0f, diagnal.y, 0.0f,
            0.0f, 0.0f, diagnal.z
        );
    }

    /*
     * A function for creating a rotation matrix that rotates a vector called
     * "from" into another vector called "to".
     * Input : from[3], to[3] which both must be *normalized* non-zero vectors
     * Output: mtx[3][3] -- a 3x3 matrix in colum-major form
     * Author: Tomas Möller, 1999
     */
    static Mat3x3<T> FromToRotation(Vec3<T> from, Vec3<T> to) {
        T h;
        T e = from.Dot(to);

        if (e > 1.0f - math::kEpsilon)     /* "from" almost or equal to "to"-vector? */
        {
            return Mat3x3<T>::identity;
        }
        else if (e < -1.0f + math::kEpsilon) /* "from" almost or equal to negated "to"? */
        {
            Vec3<T> up, left;
            T invlen;
            T fxx, fyy, fzz, fxy, fxz, fyz;
            T uxx, uyy, uzz, uxy, uxz, uyz;
            T lxx, lyy, lzz, lxy, lxz, lyz;
            /* left=CROSS(from, (1,0,0)) */
            left.x = 0.0f; left.y = from.z; left.z = -from.y;
            if (left.Dot(left) < math::kEpsilon) /* was left=CROSS(from,(1,0,0)) a good choice? */
            {
                /* here we now that left = CROSS(from, (1,0,0)) will be a good choice */
                left[0] = -from[2]; left[1] = 0.0f; left[2] = from[0];
            }
            /* normalize "left" */
            invlen = (1.0f / math::Sqrt(left.Dot(left)));

            left[0] *= invlen;
            left[1] *= invlen;
            left[2] *= invlen;
            up = left.Cross(from);
            /* now we have a coordinate system, i.e., a basis;    */
            /* M=(from, up, left), and we want to rotate to:      */
            /* N=(-from, up, -left). This is done with the matrix:*/
            /* N*M^T where M^T is the transpose of M              */
            fxx = -from[0] * from[0]; fyy = -from[1] * from[1]; fzz = -from[2] * from[2];
            fxy = -from[0] * from[1]; fxz = -from[0] * from[2]; fyz = -from[1] * from[2];

            uxx = up[0] * up[0]; uyy = up[1] * up[1]; uzz = up[2] * up[2];
            uxy = up[0] * up[1]; uxz = up[0] * up[2]; uyz = up[1] * up[2];

            lxx = -left[0] * left[0]; lyy = -left[1] * left[1]; lzz = -left[2] * left[2];
            lxy = -left[0] * left[1]; lxz = -left[0] * left[2]; lyz = -left[1] * left[2];
            /* symmetric matrix */

            return Mat3x3<T>(
                fxx + uxx + lxx, fxy + uxy + lxy, fxz + uxz + lxz,
                fxy + uxy + lxy, fyy + uyy + lyy, fyz + uyz + lyz,
                fxz + uxz + lxz, fyz + uyz + lyz, fzz + uzz + lzz
            );
        }
        else  /* the most common case, unless "from"="to", or "from"=-"to" */
        {
            /* ...otherwise use this hand optimized version (9 mults less) */
            Vec3<T> v = from.Cross(to);
            T hvx, hvz, hvxy, hvxz, hvyz;
            h = (1.0f - e) / v.Dot(v);
            hvx = h * v[0];
            hvz = h * v[2];
            hvxy = hvx * v[1];
            hvxz = hvx * v[2];
            hvyz = hvz * v[1];

            return Mat3x3<T>(
                    e + hvx * v[0], hvxy - v[2], hvxz + v[1],
                    hvxy + v[2], e + h * v[1] * v[1], hvyz - v[0],
                    hvxz - v[1], hvyz + v[0], e + hvz * v[2]
            );
        }
    }

    static Mat3x3<T> Steiner(const Vec3<T>& v) {
        float xx = v.x * v.x;
        float yy = v.y * v.y;
        float zz = v.z * v.z;

        return Mat3x3<T>(
            yy + zz, -v.x * v.y, -v.x * v.z,
            -v.x * v.y, xx + zz, -v.y * v.z,
            -v.x * v.z, -v.y * v.z, xx + yy
        );
    }
};

template<typename T>
const Mat3x3<T> Mat3x3<T>::identity(
    (T)1, (T)0, (T)0,
    (T)0, (T)1, (T)0,
    (T)0, (T)0, (T)1
);

template<typename T>
const Mat3x3<T> Mat3x3<T>::zero(
    (T)0, (T)0, (T)0,
    (T)0, (T)0, (T)0,
    (T)0, (T)0, (T)0
);

using Matrix3x3 = Mat3x3<float>;

}
