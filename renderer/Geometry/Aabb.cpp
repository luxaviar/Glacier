#include "aabb.h"
#include <limits>
#include "render/editor/gizmos.h"

namespace glacier {

AABB AABB::Union(const AABB& a, const AABB& b) {
    AABB res;
    for (int i = 0; i < 3; ++i) {
        if (a.min[i] < b.min[i]) {
            res.min[i] = a.min[i];
        } else {
            res.min[i] = b.min[i];
        }

        if (a.max[i] > b.max[i]) {
            res.max[i] = a.max[i];
        } else {
            res.max[i] = b.max[i];
        }
    }

    return res;
}

Vec3f AABB::RotateExtents(const Vec3f& extents, const Matrix4x4& rotation) {
    Vec3f result;
    for (int i = 0; i < 3; i++)
        result[i] = math::Abs(rotation[i][0] * extents.x) + math::Abs(rotation[i][1] * extents.y) + math::Abs(rotation[i][2] * extents.z);
    return result;
}

Vec3f AABB::RotateExtents(const Vec3f& extents, const Matrix3x3& rotation) {
    Vec3f result;
    for (int i = 0; i < 3; i++)
        result[i] = math::Abs(rotation[i][0] * extents.x) + math::Abs(rotation[i][1] * extents.y) + math::Abs(rotation[i][2] * extents.z);
    return result;
}

AABB AABB::Transform(const AABB& a, const Matrix4x4& tx) {
    //it's a loosen fit
    //Vec3f extents = RotateExtents(a.Extent(), tx);
    //Vec3f center = tx.MultiplyPoint3X4(a.Center());
    //AABB result;
    //result.SetCenterAndExtent(center, extents);
    //return result;

    //https://dev.theomader.com/transform-bounding-boxes/
    auto right = tx.right();
    auto up = tx.up();
    auto forward = tx.forward();
    auto trans = tx.translation();

    Vector3 xa = right * a.min.x;
    Vector3 xb = right * a.max.x;

    Vector3 ya = up * a.min.y;
    Vector3 yb = up * a.max.y;

    Vector3 za = forward * a.min.z;
    Vector3 zb = forward * a.max.z;

    Vector3 pmin = Vector3::Min(xa, xb) + Vector3::Min(ya, yb) + Vector3::Min(za, zb) + trans;
    Vector3 pmax = Vector3::Max(xa, xb) + Vector3::Max(ya, yb) + Vector3::Max(za, zb) + trans;

    return {pmin, pmax};
}

AABB::AABB() : AABB(Vec3f{ FLT_MAX }, Vec3f{ -FLT_MAX }) {}

AABB::AABB(const Vec3f& min_, const Vec3f& max_) : min(min_), max(max_) {
}

void AABB::SetCenterAndExtent(const Vec3f& center, const Vec3f& extents) {
    min = center - extents;
    max = center + extents;
}

Vec3f AABB::Center() const {
    return (min + max) * 0.5f;
}

Vec3f AABB::Size() const {
    return (max - min);
}

Vec3f AABB::Extent() const {
    return Size() * 0.5f;
}

bool AABB::IsValid() const {
    return min != Vec3f{ FLT_MAX } || max != Vec3f{ -FLT_MAX };
}

float AABB::Area() const {
    return 2.0f * ((max.x - min.x) * (max.y - min.y) + (max.y - min.y) * (max.z - min.z) + (max.z - min.z) * (max.x - min.x));
}

float AABB::Volume() const {
    Vec3f size = max - min;
    return (size.x * size.y * size.z);
}

Vec3f AABB::ClosestPoint(const Vec3f& point) const {
    float x = math::Max(min.x, math::Min(point.x, max.x));
    float y = math::Max(min.y, math::Min(point.y, max.y));
    float z = math::Max(min.z, math::Min(point.z, max.z));
    return Vec3f(x, y, z);
}

bool AABB::Contains(const Vec3f& point) const {
    return point.x >= min.x && point.x <= max.x &&
        point.y >= min.y && point.y <= max.y &&
        point.z >= min.z && point.z <= max.z;
}

bool AABB::Contains(const Vec2f& point) const {
    return Contains({ point.x, 0, point.y });
}

bool AABB::Contains(const AABB& other) const {
    return min.x < other.min.x && min.y < other.min.y && min.z < other.min.z &&
        max.x > other.max.x && max.y > other.max.y && max.z > other.max.z;
}

bool AABB::Contains(const Vec3f& center, float radius) const {
    if (!Contains(center)) return false;

    float x = math::Min(center.x - min.x, max.x - center.x);
    float y = math::Min(center.y - min.y, max.y - center.y);
    float z = math::Min(center.z - min.z, max.z - center.z);
    float dist = math::Min(x, math::Min(y, z));
    return dist > radius;
}

bool AABB::Intersects(const AABB& other) const {
    return min.x < other.max.x && max.x > other.min.x &&
        min.y < other.max.y && max.y > other.min.y &&
        min.z < other.max.z && max.z > other.min.z;
}

bool AABB::IntersectsInclusive(const AABB& other) const {
    return min.x <= other.max.x && max.x >= other.min.x &&
        min.y <= other.max.y && max.y >= other.min.y &&
        min.z <= other.max.z && max.z >= other.min.z;
}

bool AABB::Intersects(const Ray& ray) const {
    float t;
    return Intersects(ray, -1.0f, t);
}

bool AABB::Intersects(const Ray& ray, float maxDistance, float& t) const {
    t = -1.0f;
    float tmin = (std::numeric_limits<float>::lowest)();
    float tmax = (std::numeric_limits<float>::max)();

    for (int i = 0; i < 3; ++i) {
        float invD = 1.0f / ray.direction[i]; //TODO: can compiler correct handle divided by zero? we expect inf result;
        float t0 = (min[i] - ray.origin[i]) * invD;
        float t1 = (max[i] - ray.origin[i]) * invD;
        if (invD < 0.0f) {
            float temp = t1;
            t1 = t0;
            t0 = temp;
        }

        if (t0 > tmin) tmin = t0;
        if (t1 < tmax) tmax = t1;

        if (tmax < tmin || tmax < 0.0f) {
            return false;
        }
    }

    if (maxDistance > 0.0f && tmin > maxDistance) {
        return false;
    }

    t = tmin;
    return true;
}

bool AABB::Intersects(const Frustum& frustum) const {
    return Intersects(frustum.planes.data(), (int)FrustumPlane::kCount);
}

bool AABB::Intersects(const Plane* plane, int count) const {
    const Vec3f& center = Center();// center of AABB
    const Vec3f& extent = Extent();// half-diagonal

    for (int i = 0; i < count; ++i) {
        auto& p = plane[i];
        float dist = p.DistanceFromPoint(center); // <0: negative space
        float radius = extent.Dot(p.normal.Abs());
        if (dist + radius < 0) return false; // behind clip plane
    }
    return true; // AABB intersects space bounded by planes
}

float AABB::Distance(const Vec3f& point) const {
    return ClosestPoint(point).Distance(point);
}

Vec3f AABB::FarthestPoint(const Vec3f& wdir) const {
    return Vec3f(
        wdir.x > 0.0f ? max.x : min.x,
        wdir.y > 0.0f ? max.y : min.y,
        wdir.z > 0.0f ? max.z : min.z
    );
}

AABB AABB::Expand(const Vec3f& margin) const {
    return AABB(min - margin, max + margin);
}

AABB AABB::Expand(float margin) const {
    return AABB(min - margin, max + margin);
}

void AABB::AddPoint(const Vector3& point) {
    min = Vector3::Min(min, point);
    max = Vector3::Max(max, point);
}

void AABB::AddPoint(const Vector2& point) {
    min = Vector3::Min(min, { point.x, 0, point.y });
    max = Vector3::Max(max, { point.x, 0, point.y });
}

void AABB::OnDrawGizmos() {
    render::Gizmos::Instance()->DrawCube(Center(), Extent());
}

}

