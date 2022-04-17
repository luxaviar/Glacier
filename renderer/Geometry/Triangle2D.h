#pragma once

#include "ray.h"
#include "Math/Util.h"

namespace glacier {

struct Triangle2D {
    static void Barycentric(const Vector2& p, const Vector2& a, const Vector2& b, const Vector2& c, float& u, float& v, float& w);
    static Vector2 CircumCenter(const Vector2& a, const Vector2& b, const Vector2& c);
    static Vector3 SideLength(const Vector2& a, const Vector2& b, const Vector2& c);
    static float Perimeter(const Vector2& a, const Vector2& b, const Vector2& c);
    static float Area(const Vector2& a, const Vector2& b, const Vector2& c);

    Triangle2D(const Vector2& a, const Vector2& b, const Vector2& c);

    void Barycentric(const Vector2& p, float& u, float& v, float& w) const;
    bool Contains(const Vector2& point) const;
    float ClosestEdgeDist(const Vector2& point) const;
    Vector2 ClosestPoint(const Vector2& p) const;
    Vector2 Incenter() const;
    Vector2 RandomPointInside() const;

    Vector2 CircumCenter();

    //x: a-b, y: b-c, z: c-a
    Vector3 SideLength() const;
    float Perimeter() const;
    bool IsFlat() const;

    float Area() const;

    //CW order
    Vector2 a;
    Vector2 b;
    Vector2 c;
};

}
