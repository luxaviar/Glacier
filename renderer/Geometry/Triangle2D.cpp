#include <limits>
#include <math.h>
#include "Triangle2D.h"

namespace glacier {

void Triangle2D::Barycentric(const Vector2& p, const Vector2& a, const Vector2& b, const Vector2& c, float& u, float& v, float& w) {
    // code from Crister Erickson's Real-Time Collision Detection
    Vector2 v0 = b - a;
    Vector2 v1 = c - a;
    Vector2 v2 = p - a;

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

// https://en.wikipedia.org/wiki/Circumscribed_circle - Cartesian coordinates
Vector2 Triangle2D::CircumCenter(const Vector2& a, const Vector2& b, const Vector2& c) {
    auto D = (a.x * (b.y - c.y) +
        b.x * (c.y - a.y) +
        c.x * (a.y - b.y)) * 2;
    auto x = ((a.x * a.x + a.y * a.y) * (b.y - c.y) +
        (b.x * b.x + b.y * b.y) * (c.y - a.y) +
        (c.x * c.x + c.y * c.y) * (a.y - b.y));
    auto y = ((a.x * a.x + a.y * a.y) * (c.x - b.x) +
        (b.x * b.x + b.y * b.y) * (a.x - c.x) +
        (c.x * c.x + c.y * c.y) * (b.x - a.x));

    return { x / D, y / D };
}

Vector3 Triangle2D::SideLength(const Vector2& a, const Vector2& b, const Vector2& c) {
    auto alen = (a - b).Magnitude();
    auto blen = (b - c).Magnitude();
    auto clen = (c - a).Magnitude();

    return { alen, blen, clen };
}

float Triangle2D::QuatArea(float a, float b, float c) {
    float p = (a + b + c) * (-a + b + c) * (a - b + c) * (a + b - c);
    return math::Sqrt(p);
}

Triangle2D::Triangle2D(const Vector2& a_, const Vector2& b_, const Vector2& c_) :
    a(a_),
    b(b_),
    c(c_)
{
}

void Triangle2D::Barycentric(const Vector2& p, float& u, float& v, float& w) const {
    Barycentric(p, a, b, c, u, v, w);
}

// https://stackoverflow.com/questions/2049582/how-to-determine-if-a-point-is-in-a-2d-triangle
bool Triangle2D::Contains(const Vector2& p) {
    auto s = (a.x - b.x) * (p.y - c.y) - (a.y - c.y) * (p.x - c.x);
    auto t = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);

    if ((s < 0) != (t < 0) && s != 0 && t != 0)
        return false;

    auto d = (c.x - b.x) * (p.y - b.y) - (c.y - b.y) * (p.x - b.x);
    return d == 0 || (d < 0) == (s + t <= 0);
}

float Triangle2D::ClosestEdgeDist(const Vector2& point) {
    return math::Min(math::Min(math::Abs((point - a).Dot(b - a)),
        math::Abs((point - b).Dot(c - b))),
        (point - a).Dot(c - a)
    );
}

Vector2 Triangle2D::ClosestPoint(const Vector2& p) const {
    /** The code for Triangle-float3 test is from Christer Ericson's Real-Time Collision Detection, pp. 141-142. */

    // Check if P is in vertex region outside A.
    Vector2 ab = b - a;
    Vector2 ac = c - a;
    Vector2 ap = p - a;

    auto d1 = ab.Dot(ap);
    auto d2 = ac.Dot(ap);
    if (d1 <= 0.0f && d2 <= 0.0f)
        return a; // Barycentric coordinates are (1,0,0).

    // Check if P is in vertex region outside B.
    Vector2 bp = p - b;
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
    Vector2 cp = p - c;
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

Vector3 Triangle2D::SideLength() {
    return SideLength(a, b, c);
}

Vector2 Triangle2D::Incenter() {
    auto len = SideLength();
    auto inv_len = 1.0f / (len.x + len.y + len.z);

    auto x = (len.x * a.x + len.y * b.x + len.z * c.x) * inv_len;
    auto y = (len.x * a.y + len.y * b.y + len.z * c.y) * inv_len;
    return { x, y };
}

Vector2 Triangle2D::CircumCenter() {
    return CircumCenter(a, b, c);
}

float Triangle2D::Area() {
    auto len = SideLength();
    return QuatArea(len.x, len.y, len.z) / 4.0f;
}

bool Triangle2D::IsFlat() {
    auto ab = b - a;
    auto ac = c - a;

    return math::Abs(ab.Cross(ac)) < math::kEpsilon;
}

}
