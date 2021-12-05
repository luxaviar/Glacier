#include "line.h"

namespace glacier {

Line::Line(const Vec3f& point_, const Vec3f& dir_) :
    point(point_),
    dir(dir_) {
}

Vec3f Line::ClosestPoint(const Line& line) {
    float d1, d2;
    ClosestPointLineLine(point, dir, line.point, line.dir, d1, d2);
    return point + dir * d1;
}

// http://geomalgorithms.com/a07-_distance.html
void Line::ClosestPointLineLine(const Vec3f& p0, const Vec3f& u, const Vec3f& q0, const Vec3f& v, float& d1, float& d2) {
    Vec3f w0 = p0 - q0;
    Vec3f::value_type e = w0.Dot(v);
    Vec3f::value_type b = v.Dot(u);
    Vec3f::value_type c = v.Dot(v);
    Vec3f::value_type d = w0.Dot(u);
    Vec3f::value_type a = u.Dot(u);
    Vec3f::value_type denom = a * c - b * b;
    if (denom != 0.0f)
        d1 = (e * b - d * c) / denom;
    else
        d1 = 0.0f;
    d2 = (e + d1 * b) / c;
}

}

