#pragma once

#include "Math/Vec3.h"
#include "Math/Vec2.h"

namespace glacier {

struct Ray {
    Vec3f origin;
    // direction shoud be noramlized
    Vec3f direction;

    Ray() {}

    Ray(const Vec3f& origin_, const Vec3f& direction_) :
        origin(origin_),
        direction(direction_) {
    }

    Vec3f Point(float d) const {
        return origin + direction * d;
    }
};

struct Ray2D {
    Vec2f origin;
    // direction shoud be noramlized
    Vec2f direction;

    Ray2D() {}

    Ray2D(const Vec2f& origin_, const Vec2f& direction_) :
        origin(origin_),
        direction(direction_) {
    }

    Vec2f Point(float d) const {
        return origin + direction * d;
    }
};

}
