#pragma once

#include <algorithm>
#include <optional>
#include "util.h"
#include "vec4.h"
#include "quat.h"

namespace glacier {

//row major storage & notation / column vector & post-multiplication
template<typename T>
struct Mat4x4 {
    using value_type = T;
    static constexpr int rows = 4;
    static constexpr int cols = 4;

    static const Mat4x4<T> identity;
    static const Mat4x4<T> zero;

    union {
        T value[rows][cols];
        struct { Vec4<T> r[rows]; };
        struct { Vec4<T> r0, r1, r2, r3; };
    };

    constexpr Mat4x4() {}

    constexpr Mat4x4(T m00, T m01, T m02, T m03,
                      T m10, T m11, T m12, T m13,
                      T m20, T m21, T m22, T m23,
                      T m30, T m31, T m32, T m33)
        : value{m00, m01, m02, m03,
            m10, m11, m12, m13,
            m20, m21, m22, m23,
            m30, m31, m32, m33} {
    }

    constexpr Mat4x4(const Vec4<T>& r0_, const Vec4<T>& r1_, const Vec4<T>& r2_, const Vec4<T>& r3_)
        : r0(r0_), r1(r1_), r2(r2_), r3(r3_) {
    }

    constexpr Mat4x4(const Mat4x4<T>& m) 
        : r0(m.r0), r1(m.r1), r2(m.r2), r3(m.r3) {
    }

    void Zero() {
        r0.Zero();
        r1.Zero();
        r2.Zero();
        r3.Zero();
    }

    Mat4x4<T> Transposed() const {
        return Mat4x4<T>(
            r0.x, r1.x, r2.x, r3.x,
            r0.y, r1.y, r2.y, r3.y,
            r0.z, r1.z, r2.z, r3.z,
            r0.w, r1.w, r2.w, r3.w
        );
    }

    Mat4x4<T>& Transpose() const {
        std::swap(value[0][1], value[1][0]);
        std::swap(value[0][2], value[2][0]);
        std::swap(value[0][3], value[3][0]);
        std::swap(value[1][2], value[2][1]);
        std::swap(value[1][3], value[3][1]);
        std::swap(value[2][3], value[3][2]);
        return *this;
    }

    //scale rotation
    Vec3<T> MultiplyVector(const Vec3<T>& v) const {
        return Vec3<T>(
            (r0.x * v.x + r0.y * v.y + r0.z * v.z),
            (r1.x * v.x + r1.y * v.y + r1.z * v.z),
            (r2.x * v.x + r2.y * v.y + r2.z * v.z)
        );
    }

    //scale rotation
    Vec4<T> MultiplyVector(const Vec4<T>& v) const {
        return Vec4<T>(
            (r0.x * v.x + r0.y * v.y + r0.z * v.z),
            (r1.x * v.x + r1.y * v.y + r1.z * v.z),
            (r2.x * v.x + r2.y * v.y + r2.z * v.z),
            v.w
            );
    }

    //scale rotation translate
    Vec3<T> MultiplyPoint3X4(const Vec3<T>& v) const {
        return Vec3<T>(
            (r0.x * v.x + r0.y * v.y + r0.z * v.z + r0.w),
            (r1.x * v.x + r1.y * v.y + r1.z * v.z + r1.w),
            (r2.x * v.x + r2.y * v.y + r2.z * v.z + r2.w)
        );
    }

    //scale rotation translate projection
    Vec3<T> MultiplyPoint(const Vec3<T>& v) const {
        Vec3<T> res(
            (r0.x * v.x + r0.y * v.y + r0.z * v.z + r0.w),
            (r1.x * v.x + r1.y * v.y + r1.z * v.z + r1.w),
            (r2.x * v.x + r2.y * v.y + r2.z * v.z + r2.w)
        );
        T w = (r3.x * v.x + r3.y * v.y + r3.z * v.z + r3.w);
        if (math::Abs(w) > 1.0e-7f) {
            T invW = 1.0f / w;
            res.x *= invW;
            res.y *= invW;
            res.z *= invW;
        } else {
            res.x = 0;
            res.y = 0;
            res.z = 0;
        }

        return res;
    }

    T Determinant() const {
        // | a b c d |     | f g h |     | e g h |     | e f h |     | e f g |
        // | e f g h | = a | j k l | - b | i k l | + c | i j l | - d | i j k |
        // | i j k l |     | n o p |     | m o p |     | m n p |     | m n o |
        // | m n o p |
        //
        //   | f g h |
        // a | j k l | = a ( f ( kp - lo ) - g ( jp - ln ) + h ( jo - kn ) )
        //   | n o p |
        //
        //   | e g h |     
        // b | i k l | = b ( e ( kp - lo ) - g ( ip - lm ) + h ( io - km ) )
        //   | m o p |     
        //
        //   | e f h |
        // c | i j l | = c ( e ( jp - ln ) - f ( ip - lm ) + h ( in - jm ) )
        //   | m n p |
        //
        //   | e f g |
        // d | i j k | = d ( e ( jo - kn ) - f ( io - km ) + g ( in - jm ) )
        //   | m n o |
        //
        // Cost of operation
        // 17 adds and 28 muls.
        //
        // add: 6 + 8 + 3 = 17
        // mul: 12 + 16 = 28

        T a = value[0][0], b = value[0][1], c = value[0][2], d = value[0][3];
        T e = value[1][0], f = value[1][1], g = value[1][2], h = value[1][3];
        T i = value[2][0], j = value[2][1], k = value[2][2], l = value[2][3];
        T m = value[3][0], n = value[3][1], o = value[3][2], p = value[3][3];

        T kp_lo = k * p - l * o;
        T jp_ln = j * p - l * n;
        T jo_kn = j * o - k * n;
        T ip_lm = i * p - l * m;
        T io_km = i * o - k * m;
        T in_jm = i * n - j * m;

        return a * (f * kp_lo - g * jp_ln + h * jo_kn) -
            b * (e * kp_lo - g * ip_lm + h * io_km) +
            c * (e * jp_ln - f * ip_lm + h * in_jm) -
            d * (e * jo_kn - f * io_km + g * in_jm);
    }

    Mat4x4<T>& InvertOrthonormal() {
        std::swap(r0.y, r1.x);
        std::swap(r0.z, r2.x);
        std::swap(r1.z, r2.y);

        Vec3<T> t(-r0.w, -r1.w, -r2.w);
        Vec3<T> trans = MultiplyVector(t);
        r0.w = trans.x;
        r1.w = trans.y;
        r2.w = trans.z;

        return *this;
    }

    Mat4x4<T> InvertedOrthonormal() const {
        Mat4x4<T> mat{*this};
        mat.InvertOrthonormal();
        return mat;
    }

    //https://referencesource.microsoft.com/#System.Numerics/System/Numerics/Matrix4x4.cs,48ce53b7e55d0436
    std::optional<Mat4x4<T>> Inverted() const {
        // Cost of operation
        // 53 adds, 104 muls, and 1 div.
        T a = value[0][0], b = value[0][1], c = value[0][2], d = value[0][3];
        T e = value[1][0], f = value[1][1], g = value[1][2], h = value[1][3];
        T i = value[2][0], j = value[2][1], k = value[2][2], l = value[2][3];
        T m = value[3][0], n = value[3][1], o = value[3][2], p = value[3][3];

        T kp_lo = k * p - l * o;
        T jp_ln = j * p - l * n;
        T jo_kn = j * o - k * n;
        T ip_lm = i * p - l * m;
        T io_km = i * o - k * m;
        T in_jm = i * n - j * m;

        T a11 = +(f * kp_lo - g * jp_ln + h * jo_kn);
        T a12 = -(e * kp_lo - g * ip_lm + h * io_km);
        T a13 = +(e * jp_ln - f * ip_lm + h * in_jm);
        T a14 = -(e * jo_kn - f * io_km + g * in_jm);

        T det = a * a11 + b * a12 + c * a13 + d * a14;

        if (math::Abs(det) < math::kEpsilon) {
            return {};
        }

        T invDet = 1.0f / det;

        Mat4x4<T> inv_mat;
        inv_mat[0][0] = a11 * invDet;
        inv_mat[1][0] = a12 * invDet;
        inv_mat[2][0] = a13 * invDet;
        inv_mat[3][0] = a14 * invDet;

        inv_mat[0][1] = -(b * kp_lo - c * jp_ln + d * jo_kn) * invDet;
        inv_mat[1][1] = +(a * kp_lo - c * ip_lm + d * io_km) * invDet;
        inv_mat[2][1] = -(a * jp_ln - b * ip_lm + d * in_jm) * invDet;
        inv_mat[3][1] = +(a * jo_kn - b * io_km + c * in_jm) * invDet;

        T gp_ho = g * p - h * o;
        T fp_hn = f * p - h * n;
        T fo_gn = f * o - g * n;
        T ep_hm = e * p - h * m;
        T eo_gm = e * o - g * m;
        T en_fm = e * n - f * m;

        inv_mat[0][2] = +(b * gp_ho - c * fp_hn + d * fo_gn) * invDet;
        inv_mat[1][2] = -(a * gp_ho - c * ep_hm + d * eo_gm) * invDet;
        inv_mat[2][2] = +(a * fp_hn - b * ep_hm + d * en_fm) * invDet;
        inv_mat[3][2] = -(a * fo_gn - b * eo_gm + c * en_fm) * invDet;

        T gl_hk = g * l - h * k;
        T fl_hj = f * l - h * j;
        T fk_gj = f * k - g * j;
        T el_hi = e * l - h * i;
        T ek_gi = e * k - g * i;
        T ej_fi = e * j - f * i;

        inv_mat[0][3] = -(b * gl_hk - c * fl_hj + d * fk_gj) * invDet;
        inv_mat[1][3] = +(a * gl_hk - c * el_hi + d * ek_gi) * invDet;
        inv_mat[2][3] = -(a * fl_hj - b * el_hi + d * ej_fi) * invDet;
        inv_mat[3][3] = +(a * fk_gj - b * ek_gi + c * ej_fi) * invDet;

        return inv_mat;
    }

    T operator()(int row, int col) const {
        return value[row][col];
    }

    T& operator()(int row, int col) {
        return value[row][col];
    }

    const Vec4<T>& operator[](int index) const {
        return r[index];
    }

    Vec4<T>& operator[](int index) {
        return r[index];
    }

    Mat4x4<T>& operator =(const Mat4x4<T>& v) {
        r0 = v.r0;
        r1 = v.r1;
        r2 = v.r2;
        r3 = v.r3;

        return *this;
    }

    Mat4x4<T>& operator *=(T v) {
        r0 *= v;
        r1 *= v;
        r2 *= v;
        r3 *= v;
        return *this;
    }

    Mat4x4<T>& operator -=(const Mat4x4<T>& v) {
        r0 -= v.r0;
        r1 -= v.r1;
        r2 -= v.r2;
        r3 -= v.r3;
        return *this;
    }

    Mat4x4<T>& operator +=(const Mat4x4<T>& v) {
        r0 += v.r0;
        r1 += v.r1;
        r2 += v.r2;
        r3 += v.r3;
        return *this;
    }
    
    Mat4x4<T> operator +(const Mat4x4<T>& v) {
        return Mat4x4<T>(
            r0 + v.r0,
            r1 + v.r1,
            r2 + v.r2,
            r3 + v.r3
        );
    }

    Mat4x4<T>& operator *=(const Mat4x4<T>& v) {
        r0 = v.r0 * r0.x + v.r1 * r0.y + v.r2 * r0.z + v.r3 * r0.w;
        r1 = v.r0 * r1.x + v.r1 * r1.y + v.r2 * r1.z + v.r3 * r1.w;
        r2 = v.r0 * r2.x + v.r1 * r2.y + v.r2 * r2.z + v.r3 * r2.w;
        r3 = v.r0 * r3.x + v.r1 * r3.y + v.r2 * r3.z + v.r3 * r3.w;
        return *this;
    }

    Mat4x4<T> operator *(const Mat4x4<T>& v) const {
        return Mat4x4<T>(
            v.r0 * r0.x + v.r1 * r0.y + v.r2 * r0.z + v.r3 * r0.w,
            v.r0 * r1.x + v.r1 * r1.y + v.r2 * r1.z + v.r3 * r1.w,
            v.r0 * r2.x + v.r1 * r2.y + v.r2 * r2.z + v.r3 * r2.w,
            v.r0 * r3.x + v.r1 * r3.y + v.r2 * r3.z + v.r3 * r3.w
        );
    }
    
    Vec4<T> operator *(const Vec4<T>& v) const {
        return Vec4<T>(
            v.x * r0.x + v.y * r0.y + v.z * r0.z + v.w * r0.w,
            v.x * r1.x + v.y * r1.y + v.z * r1.z + v.w * r1.w,
            v.x * r2.x + v.y * r2.y + v.z * r2.z + v.w * r2.w,
            v.x * r3.x + v.y * r3.y + v.z * r3.z + v.w * r3.w
        );
    }

    Mat4x4<T> operator *(T v) const {
        return Mat4x4<T>(
            r0 * v,
            r1 * v,
            r2 * v,
            r3 * v
        );
    }

    operator T* () {
        return (T*)value;
    }

    operator const T* () const {
        return (const T*)value;
    }

    Vec3<T> right() const {
        return { value[0][0], value[1][0], value[2][0] };
    }

    Vec3<T> up() const {
        return { value[0][1], value[1][1], value[2][1] };
    }

    Vec3<T> forward() const {
        return { value[0][2], value[1][2], value[2][2] };
    }

    Vec3<T> translation() const {
        return { value[0][3], value[1][3], value[2][3] };
    }

    //https://math.stackexchange.com/questions/237369/given-this-transformation-matrix-how-do-i-decompose-it-into-translation-rotati
    //make sure it's a transform matrix
    void Decompose(Vec3<T>& pos, Quat<T>& rot, Vec3<T>& scale) const {
        pos.x = value[0][3];
        pos.y = value[1][3];
        pos.z = value[2][3];
           
        /* extract the columns of the matrix. */
        Vec3<T> col0{ value[0][0], value[1][0], value[2][0] };
        Vec3<T> col1{ value[0][1], value[1][1], value[2][1] };
        Vec3<T> col2{ value[0][2], value[1][2], value[2][2] };

        /* extract the scaling factors */
        scale.x = col0.Magnitude();
        scale.y = col1.Magnitude();
        scale.z = col2.Magnitude();
            
        /* and the sign of the scaling */
        if (Determinant() < 0) scale = -scale;

        /* and remove all scaling from the matrix */
        if (scale.x) col0 /= scale.x;
        if (scale.y) col1 /= scale.y;
        if (scale.z) col2 /= scale.z;
        
        Mat3x3<T> rot_mat{
            col0.x, col1.x, col2.x,
            col0.y, col1.y, col2.y,
            col0.z, col1.z, col2.z,
        };

        rot = Quat<T>::FromMatrix(rot_mat);
    }

    static Mat4x4<T> TR(const Vec3<T>& pos, const Mat3x3<T>& rot) {
        return Mat4x4<T>(
            rot.r0.x, rot.r0.y, rot.r0.z, pos.x,
            rot.r1.x, rot.r1.y, rot.r1.z, pos.y,
            rot.r2.x, rot.r2.y, rot.r2.z, pos.z,
            0.0f, 0.0f, 0.0f, 1.0f
            );
    }

    static Mat4x4<T> TR(const Vec3<T>& pos, const Quat<T>& rot) {
        return TR(pos, rot.ToMatrix());
    }

    static Mat4x4<T> TRS(const Vec3<T>& pos, const Quat<T>& rot, const Vec3<T>& scale) {
        float x = rot.x + rot.x;
        float y = rot.y + rot.y;
        float z = rot.z + rot.z;
        float xx = rot.x * x;
        float yy = rot.y * y;
        float zz = rot.z * z;
        float xy = rot.x * y;
        float xz = rot.x * z;
        float yz = rot.y * z;
        float wx = rot.w * x;
        float wy = rot.w * y;
        float wz = rot.w * z;

        return Mat4x4<T>(
            (1.0f - (yy + zz)) * scale.x, (xy - wz) * scale.y, (xz + wy) * scale.z, pos.x,
            (xy + wz) * scale.x, (1.0f - (xx + zz)) * scale.y, (yz - wx) * scale.z, pos.y,
            (xz - wy) * scale.x, (yz + wx) * scale.y, (1.0f - (xx + yy)) * scale.z, pos.z,
            0.0f, 0.0f, 0.0f, 1.0f
        );
    }

    static Mat4x4<T> Translate(const Vec3<T>& pos) {
        return Mat4x4<T>(
            1.0f, 0.0f, 0.0f, pos.x,
            0.0f, 1.0f, 0.0f, pos.y,
            0.0f, 0.0f, 1.0f, pos.z,
            0.0f, 0.0f, 0.0f, 1.0f
            );
    }

    static Mat4x4<T> Rotation(const Quat<T>& rot) {
        T x = rot.x + rot.x;
        T y = rot.y + rot.y;
        T z = rot.z + rot.z;
        T xx = rot.x * x;
        T yy = rot.y * y;
        T zz = rot.z * z;
        T xy = rot.x * y;
        T xz = rot.x * z;
        T yz = rot.y * z;
        T wx = rot.w * x;
        T wy = rot.w * y;
        T wz = rot.w * z;

        return Mat4x4<T>(
            1.0f - (yy + zz), xy - wz, xz + wy, 0.0f,
            xy + wz, 1.0f - (xx + zz), yz - wx, 0.0f,
            xz - wy, yz + wx, 1.0f - (xx + yy), 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
            );
    }

    static Mat4x4<T> Scale(const Vec3<T>& scale) {
        return Mat4x4<T>(
            scale.x, 0.0f, 0.0f, 0.0f,
            0.0f, scale.y, 0.0f, 0.0f,
            0.0f, 0.0f, scale.z, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
            );
    }

    static Mat4x4<T> LookAtLH(const Vec3<T>& pos, const Vec3<T>& target, const Vec3<T>& up) {
        auto dir = (target - pos).Normalized();
        return LookToLH(pos, dir, up);
    }

    static Mat4x4<T> LookToLH(const Vec3<T>& pos, const Vec3<T>& dir, const Vec3<T>& up) {
        auto forward = dir.Normalized(); //g
        auto right = up.Cross(forward).Normalized(); //r = t x g = -g x t
        auto nup = forward.Cross(right).Normalized(); //u = g x r = r x -g

        auto mat = Mat4x4<T>(
            right.x, right.y, right.z, -pos.Dot(right),
            nup.x, nup.y, nup.z, -pos.Dot(nup),
            forward.x, forward.y, forward.z, -pos.Dot(forward),
            0, 0, 0, 1
        );

        return mat;
    }

    //https://docs.microsoft.com/en-us/windows/win32/dxtecharts/the-direct3d-transformation-pipeline
    static Mat4x4<T> PerspectiveFovLH(float fov, float aspect, float n, float f) {
        float half_cot = 1.0f / std::tan(fov / 2.0f);
        float reverse_diff = 1.0f / (f - n);

        auto mat = Mat4x4<T>(
            half_cot / aspect, 0, 0, 0,
            0, half_cot, 0, 0,
            0, 0, f * reverse_diff, -n * f * reverse_diff,
            0, 0, 1.0f, 0
        );

        return mat;
    }

    static Mat4x4<T> OrthoLH(float width, float height, float n, float f) {
        float reciprocal_range = 1.0f / (f - n);

        auto mat = Mat4x4<T>(
            2.0f / width, 0, 0, 0,
            0, 2.0f / height, 0, 0,
            0, 0, reciprocal_range, -n * reciprocal_range,
            0, 0, 0.0, 1.0f
            );

        return mat;
    }

    static Mat4x4<T> OrthoOffCenterLH(float left, float right, float buttom, float top, float n, float f) {
        float reciprocal_width = 1.0f / (right - left);
        float reciprocal_height = 1.0f / (top - buttom);
        float reciprocal_range = 1.0f / (f - n);

        auto mat = Mat4x4<T>(
            reciprocal_width + reciprocal_width, 0, 0, -(left + right) * reciprocal_width,
            0, reciprocal_height + reciprocal_height, 0, -(top + buttom) * reciprocal_height,
            0, 0, reciprocal_range, -n * reciprocal_range,
            0, 0, 0.0, 1.0f
            );

        return mat;
    }
};

template<typename T>
const Mat4x4<T> Mat4x4<T>::identity(
    (T)1, (T)0, (T)0, (T)0,
    (T)0, (T)1, (T)0, (T)0,
    (T)0, (T)0, (T)1, (T)0,
    (T)0, (T)0, (T)0, (T)1
);

template<typename T>
const Mat4x4<T> Mat4x4<T>::zero(
    (T)0, (T)0, (T)0, (T)0,
    (T)0, (T)0, (T)0, (T)0,
    (T)0, (T)0, (T)0, (T)0,
    (T)0, (T)0, (T)0, (T)0
);

using Matrix4x4 = Mat4x4<float>;

}
