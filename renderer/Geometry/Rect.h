#pragma once

#include "Math/Vec2.h"
#include "Math/Vec3.h"

namespace glacier {

struct Rect {
    Rect(const Vec2f& a_, const Vec2f& b_, const Vec2f& c_);
    Rect(const Vec2f& center, const Vec2f& forward, float half_width, float half_height);
    bool Contains(const Vec2f& point) const;
    bool Contains(const Vec3f& point) const;

    Vec2f a;
    Vec2f b;
    Vec2f c;
};

}
