#pragma once

#include "ray.h"
#include "Math/Util.h"

namespace glacier {

struct Triangle2D {
    static void Barycentric(const Vector2& p, const Vector2& a, const Vector2& b, const Vector2& c, float& u, float& v, float& w);
    static Vector2 CircumCenter(const Vector2& a, const Vector2& b, const Vector2& c);
    static Vector3 SideLength(const Vector2& a, const Vector2& b, const Vector2& c);
    //4 * Area
    static float QuatArea(float a, float b, float c);

    Triangle2D(const Vector2& a, const Vector2& b, const Vector2& c);

    void Barycentric(const Vector2& p, float& u, float& v, float& w) const;
    bool Contains(const Vector2& point);
    float ClosestEdgeDist(const Vector2& point);
    //bool Contains(const Vec3f& point);
    //bool Intersects(const Ray2D& ray, float& t) const;
    //bool Intersects(const Ray2D& ray, float max, float& t) const;
    Vector2 ClosestPoint(const Vector2& p) const;
    Vector2 Incenter();

    Vector2 CircumCenter();

    //x: a-b, y: b-c, z: c-a
    Vector3 SideLength();
    bool IsFlat();

    float Area();

    //CW order
    Vector2 a;
    Vector2 b;
    Vector2 c;
};

}
