#pragma once

#include "Math/Vec2.h"
#include "Math/Vec3.h"
#include "Math/Util.h"
#include "Ray.h"
#include "LineSegment.h"

namespace glacier {

struct Circle {
    Circle(const Vec2f& center_, float radius_);

    bool Contains(const Vec2f& point) const;
    bool Contains(const Vec3f& point) const;

    bool Intersect(const Ray2D& ray, float max_t, float& t);
    bool Intersect(const LineSegment2D& line);

    bool Squeeze(const Vec2f& fixed_center, const Circle& fixed_other, float& t);

    Vec2f center;
    float radius;
    //float radius_sq;
};

}

