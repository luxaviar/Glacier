#include "circle.h"

namespace glacier {

Circle::Circle(const Vec2f& center_, float radius_) :
    center(center_),
    radius(radius_)
    //radius_sq(radius_ * radius_)
{
}

bool Circle::Contains(const Vec2f& point) const {
    return (point - center).MagnitudeSq() <= radius * radius;
}

bool Circle::Contains(const Vec3f& point) const {
    Vec2f p(point.x, point.z);
    return Contains(p);
}

bool Circle::Intersect(const Ray2D& ray, float max_t, float& t) {
    auto radius_sq = radius * radius;
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

//https://stackoverflow.com/questions/1073336/circle-line-segment-collision-detection-algorithm
bool Circle::Intersect(const LineSegment2D& line) {
    auto ab = line.b - line.a;
    auto ca = line.a - center;
    
    float a = ab.Dot(ab);
    float b = 2 * ca.Dot(ab);
    float c = ca.Dot(ca) - radius * radius;

    float discriminant = b * b - 4 * a * c;
    if (discriminant < 0) {
        return false;
    }

    discriminant = sqrt(discriminant);

    // either solution may be on or off the ray so need to test both
    // t1 is always the smaller value, because BOTH discriminant and
    // a are nonnegative.
    float t1 = (-b - discriminant) / (2 * a);
    float t2 = (-b + discriminant) / (2 * a);

    // 3x HIT cases:
    //          -o->             --|-->  |            |  --|->
    // Impale(t1 hit,t2 hit), Poke(t1 hit,t2>1), ExitWound(t1<0, t2 hit), 

    // 3x MISS cases:
    //       ->  o                     o ->              | -> |
    // FallShort (t1>1,t2>1), Past (t1<0,t2<0), CompletelyInside(t1<0, t2>1)

    if (t1 < 0 && t2 > 1) {
        // CompletelyInside
        return true;
    }

    if (t1 >= 0 && t1 <= 1)
    {
        // t1 is the intersection, and it's closer than t2
        // (since t1 uses -b - discriminant)
        // Impale, Poke
        return true;
    }

    // here t1 didn't intersect so we are either started
    // inside the sphere or completely past it
    if (t2 >= 0 && t2 <= 1)
    {
        // ExitWound
        return true;
    }

    // no intn: FallShort, Past
    return false;
}

bool Circle::Squeeze(const Vec2f& fixed_center, const Circle& fixed_other, float& t) {
    auto oc = fixed_other.center - center;
    auto fc = (fixed_center - center).Normalized();
    //if (fixed_center.AlmostEquals(center)) {
    //    fc = Vec2f::right;
    //}

    float dist_sq = oc.MagnitudeSq();
    float detach_sq = (radius + fixed_other.radius) * (radius + fixed_other.radius);
    if (dist_sq >= detach_sq) {
        return false;
    }

    float project = oc.Dot(fc);
    float h2 = dist_sq - project * project;
    float w = math::Sqrt(detach_sq - h2);
    t = w - project;

    //if (::isnan(t)) {
    //    return false;
    //}

    return true;
}

}

