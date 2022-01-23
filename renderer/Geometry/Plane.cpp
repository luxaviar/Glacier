#include "Plane.h"

namespace glacier {

Plane::Plane(const Vec3f& plane_normal, const Vec3f& point_on_plane)
{
    // Guarantee a normal.  This constructor isn't meant to be called frequently, but if it is, we can change this.
    normal = plane_normal.Normalized();
    d = -point_on_plane.Dot(normal);
}

Plane::Plane(const Vec3f& a, const Vec3f& b, const Vec3f& c) {
    normal = (b - a).Cross(c - a).Normalized();
    d = a.Dot(normal);
}

bool Plane::Intersect(Ray& ray, float max, float& t) {
    float dist = DistanceFromPoint(ray.origin);
    if (math::AlmostEqual(dist, 0.0f)) { // on the plane
        t = 0.0f;
        return true;
    }

    float cos = ray.direction.Dot(normal);
    if (math::AlmostEqual(cos, 0.0f)) { //parallel
        return false;
    }

    if ((dist > 0 && cos > 0) || (dist < 0 && cos < 0)) { //diverge direction
        return false;
    }

    t = math::Abs(dist / cos);
    return true;
}

}
