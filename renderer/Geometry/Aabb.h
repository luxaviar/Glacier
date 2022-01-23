#pragma once

#include "ray.h"
#include "Math/Vec3.h"
#include "Math/Vec2.h"
#include "Math/Mat4.h"
#include "plane.h"

namespace glacier {

struct AABB {
    static AABB Union(const AABB& a, const AABB& b);
    static Vec3f RotateExtents(const Vec3f& extents, const Matrix4x4& rotation);
    static Vec3f RotateExtents(const Vec3f& extents, const Matrix3x3& rotation);
    static AABB Transform(const AABB& a, const Matrix4x4& tx);

    AABB();
    AABB(const Vec3f& min_, const Vec3f& max_);

    void SetCenterAndExtent(const Vec3f& center, const Vec3f& extents);

    Vec3f Center() const;
    Vec3f Size() const;
    Vec3f Extent() const;
    bool IsValid() const;

    float Area() const;
    float Volume() const;

    Vec3f ClosestPoint(const Vec3f& point) const;
    bool Contains(const Vec3f& point) const;
    bool Contains(const Vec2f& point) const;
    bool Contains(const AABB& other) const;
    bool Contains(const Vec3f& center, float radius) const;

    bool Intersects(const AABB& other) const;
    bool IntersectsInclusive(const AABB& other) const;
    bool Intersects(const Ray& ray) const;
    bool Intersects(const Ray& ray, float maxDistance, float& t) const;
    bool Intersects(const Plane* plane, int count) const;

    float Distance(const Vec3f& point) const;
    Vec3f FarthestPoint(const Vec3f& wdir) const;
        
    AABB Expand(const Vec3f& margin) const;
    AABB Expand(float margin) const;
    void AddPoint(const Vector3& point);
    void AddPoint(const Vector2& point);

    void OnDrawGizmos();

    Vec3f min;
    Vec3f max;
};

}
