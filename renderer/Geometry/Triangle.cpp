#include <limits>
#include <math.h>
#include "triangle.h"

namespace glacier {

void Triangle::Barycentric(const Vec3f& p, const Vec3f& a, const Vec3f& b, const Vec3f& c, float& u, float& v, float& w) {
    // code from Crister Erickson's Real-Time Collision Detection
    Vec3f v0 = b - a;
    Vec3f v1 = c - a;
    Vec3f v2 = p - a;

    auto d00 = v0.Dot(v0);
    auto d01 = v0.Dot(v1);
    auto d11 = v1.Dot(v1);
    auto d20 = v2.Dot(v0);
    auto d21 = v2.Dot(v1);
    auto invDenom = 1.0f / (d00 * d11 - d01 * d01);

    v = (d11 * d20 - d01 * d21) * invDenom;
    w = (d00 * d21 - d01 * d20) * invDenom;
    u = 1.0f - v - w;
}

Triangle::Triangle(const Vec3f& a_, const Vec3f& b_, const Vec3f& c_) :
    a(a_),
    b(b_),
    c(c_),
    normal_(Vec3f::zero) {
}

Vec3f Triangle::Normal() {
    if (normal_ == Vec3f::zero) {
        normal_ = (b - a).Cross(c - a).Normalized();
    }

    return normal_;
}

void Triangle::Barycentric(const Vec3f& p, float& u, float& v, float& w) const {
    Barycentric(p, a, b, c, u, v, w);
}

bool Triangle::InPositiveSpace(const Vec3f& point) {
    return (point - a).Dot(Normal()) >= 0;
}

bool Triangle::Contains(const Vec3f& point, float threshold) {
    Vec3f normal = Normal();
    float dist = (point - a).Dot(normal);
    if (math::Abs(dist) > threshold) {
        return false;
    }

    float u, v, w;
    Barycentric(normal * dist, u, v, w);
    if (::isnan(u) || ::isnan(v) || ::isnan(w)) {
        return false;
    }

    if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f || w < 0.0f || w > 1.0f) {
        return false;
    }

    return true;
}

float Triangle::ClosestEdgeDist(const Vec3f& point) {
    return math::Min(math::Min(math::Abs((point - a).Dot(b - a)),
        math::Abs((point - b).Dot(c - b))),
        (point - a).Dot(c - a)
    );
}

bool Triangle::Intersects(const Ray& ray, float& t) const {
    return Intersects(ray, std::numeric_limits<float>::max(), t);
}

bool Triangle::Intersects(const Ray& ray, float max, float& t) const {
    // E1
    Vec3f E1 = b - a;

    // E2
    Vec3f E2 = c - a;

    // P
    Vec3f P = ray.direction.Cross(E2);

    // determinant
    auto det = E1.Dot(P);

    // keep det > 0, modify T accordingly
    Vec3f T;
    if (det > 0.0f) {
        T = ray.origin - a;
    }
    else {
        T = a - ray.origin;
        det = -det;
    }

        
    // If determinant is near zero, ray lies in plane of triangle
    if (det < math::kEpsilon) {
        t = -1.0f;
        return false;
    }

    //这里实际上u还没有计算完毕，此时的值是Dot(P,T)，如果Dot(P,T) > det, 那么u > 1，无交点
    // Calculate u and make sure u <= 1
    auto u = T.Dot(P);
    if (u < 0.0f || u > det) {
        t = -1.0f;
        return false;
    }

    // Q
    Vec3f Q = T.Cross(E1);

    //要确保u + v <= 1，也即 [Dot(P,T) + Dot(Q, D)] / det 必须不能大于1
    // Calculate v and make sure u + v <= 1
    auto v = ray.direction.Dot(Q);
    if (v < 0.0f || u + v > det) {
        t = -1.0f;
        return false;
    }

    // Calculate t, scale parameters, ray intersects triangle
    t = E2.Dot(Q);

    auto fInvDet = 1.0f / det;
    t *= fInvDet;
    //u *= fInvDet;
    //v *= fInvDet;

    if (t < 0.0f || t > max) {
        return false;
    }

    return true;
}

Vec3f Triangle::ClosestPoint(const Vec3f& p) const {
    /** The code for Triangle-float3 test is from Christer Ericson's Real-Time Collision Detection, pp. 141-142. */

    // Check if P is in vertex region outside A.
    Vec3f ab = b - a;
    Vec3f ac = c - a;
    Vec3f ap = p - a;

    auto d1 = ab.Dot(ap);
    auto d2 = ac.Dot(ap);
    if (d1 <= 0.0f && d2 <= 0.0f)
        return a; // Barycentric coordinates are (1,0,0).

    // Check if P is in vertex region outside B.
    Vec3f bp = p - b;
    auto d3 = ab.Dot(bp);
    auto d4 = ac.Dot(bp);
    if (d3 >= 0.0f && d4 <= d3)
        return b; // Barycentric coordinates are (0,1,0).

    // Check if P is in edge region of AB, and if so, return the projection of P onto AB.
    auto vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        auto v = d1 / (d1 - d3);
        return a + ab * v; // The barycentric coordinates are (1-v, v, 0).
    }

    // Check if P is in vertex region outside C.
    Vec3f cp = p - c;
    auto d5 = ab.Dot(cp);
    auto d6 = ac.Dot(cp);
    if (d6 >= 0.0f && d5 <= d6)
        return c; // The barycentric coordinates are (0,0,1).

    // Check if P is in edge region of AC, and if so, return the projection of P onto AC.
    auto vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        auto w = d2 / (d2 - d6);
        return a + ac * w; // The barycentric coordinates are (1-w, 0, w).
    }

    // Check if P is in edge region of BC, and if so, return the projection of P onto BC.
    auto va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && d4 - d3 >= 0.0f && d5 - d6 >= 0.0f) {
        auto w = (d4 - d3) / (d4 - d3 + d5 - d6);
        return b + (c - b) * w; // The barycentric coordinates are (0, 1-w, w).
    }

    // P must be inside the face region. Compute the closest point through its barycentric coordinates (u,v,w).
    auto denom = 1.0f / (va + vb + vc);
    auto vv = vb * denom;
    auto ww = vc * denom;
    return a + ab * vv + ac * ww;
}

// https://gamedev.stackexchange.com/questions/60630/how-do-i-find-the-circumcenter-of-a-triangle-in-3d
Vec3f Triangle::CircumSphereCenter() {
    Vector3 ac = c - a;
    Vector3 ab = b - a;
    Vector3 abXac = ab.Cross(ac);

    // this is the vector from a TO the circumsphere center
    Vector3 toCircumsphereCenter = (abXac.Cross(ab) * ac.MagnitudeSq() + ac.Cross(abXac) * ab.MagnitudeSq()) / (2.f * abXac.MagnitudeSq());
    float circumsphereRadius = toCircumsphereCenter.Magnitude();

    // The 3 space coords of the circumsphere center then:
    Vector3 ccs = a + toCircumsphereCenter; // now this is the actual 3space location
    return ccs;
}

}

