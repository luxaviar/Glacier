#pragma once

#include "Math/Vec3.h"

namespace glacier {

struct Line {
    static void ClosestPointLineLine(const Vec3f& p0, const Vec3f& u, const Vec3f& q0, const Vec3f& v, float& d1, float& d2);

    Line(const Vec3f& point, const Vec3f& dir);
    Vec3f ClosestPoint(const Line& line);

    Vec3f point;
    Vec3f dir;
};

}
