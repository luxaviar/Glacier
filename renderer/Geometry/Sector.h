#pragma once

#include "Math/Vec2.h"
#include "Math/Vec3.h"

namespace glacier {

struct Sector {
    Sector(const Vec2f& center_, const Vec2f& dir_, float radius_, float half_angle);
    bool Contains(const Vec2f& point) const;
    bool Contains(const Vec3f& point) const;

    Vec2f center;
    float radius;
    float half_angle;

    Vec2f left;
    Vec2f right;
};

}
