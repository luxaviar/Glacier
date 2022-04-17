#pragma once

#include "ray.h"
#include "line.h"

namespace glacier {

struct LineSegment {
    static void ClosestPointSegmentSegment(const Vec3f& p0, const Vec3f& p1, const Vec3f& q0, const Vec3f& q1, float& d1, float& d2);

    LineSegment() {}
    LineSegment(const Vec3f& a, const Vec3f& b);

    Vec3f Center() const;
    Vec3f::value_type Length() const;
    Vec3f ClosestPoint(const Vec3f& point, float &d) const;
    Vec3f ClosestPoint(const Vec3f& point) const;
    Vec3f ClosestPoint(const Line& line) const;
    Vec3f ClosestPoint(const Ray& ray) const;
    Vec3f::value_type Distance(const Vec3f& point) const;
    Vec3f::value_type DistanceSq(const Vec3f& point) const;
    bool Contains(const Vec3f& point) const;
    bool Intersects(const Ray& ray, float max, float& t) const;

    Vec3f a;
    Vec3f b;
};

struct LineSegment2D {
    LineSegment2D(const Vector2& a, const Vector2& b) : a(a), b(b) {}

    bool Intersects(const LineSegment2D& other, float* t = nullptr) const;

    Vector2 ClosestPoint(const Vector2& point, float& d) const;
    Vector2 ClosestPoint(const Vector2& point) const;

    float Distance(const Vector2& point) const;
    float DistanceSq(const Vector2& point) const;

    Vector2 a;
    Vector2 b;
};

}
