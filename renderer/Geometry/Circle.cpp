#include "circle.h"

namespace glacier {

Circle::Circle(const Vec2f& center_, float radius_) :
    center(center_),
    radius(radius_),
    radius_sq(radius_ * radius_)
{
}

bool Circle::Contains(const Vec2f& point) const {
    return (point - center).MagnitudeSq() <= radius_sq;
}

bool Circle::Contains(const Vec3f& point) const {
    Vec2f p(point.x, point.z);
    return Contains(p);
}

bool Circle::Intersect(const Ray2D& ray, float max_t, float& t) {
    Vec2f oc = center - ray.origin;
    float len_sq = oc.MagnitudeSq();
    //inside circle
    if (len_sq <= radius_sq) {
        t = 0.0f;
        return true;
    }

    float project = oc.Dot(ray.direction);
    //behind ray origin
    if (project < 0.0) {
        return false;
    }

    float h_sq = len_sq - project * project;
    if (h_sq > radius_sq) {
        return false;
    }

    float w = math::Sqrt(radius_sq - h_sq);
    t = project - w;
    return true;
}

bool Circle::Squeeze(const Vec2f& fixed_center, const Circle& fixed_other, float& t) {
    auto oc = fixed_other.center - center;
    auto fc = (fixed_center - center).Normalized();
    float dist_sq = oc.MagnitudeSq();
    float detach_sq = (radius + fixed_other.radius) * (radius + fixed_other.radius);
    if (dist_sq >= detach_sq) {
        return false;
    }

    float project = oc.Dot(fc);
    float h2 = dist_sq - project * project;
    float w = math::Sqrt(detach_sq - h2);
    t = w - project;

    return true;
}

}

