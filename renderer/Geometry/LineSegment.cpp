#include "linesegment.h"
#include "Math/Util.h"
#include "line.h"

namespace glacier {

LineSegment::LineSegment(const Vec3f& a_, const Vec3f& b_) :
    a(a_),
    b(b_) {
}

Vec3f LineSegment::Center() const {
    return a + (b - a) / (Vec3f::value_type)2.0f;
}

Vec3f::value_type LineSegment::Length() const {
    return (b - a).Magnitude();
}

Vec3f LineSegment::ClosestPoint(const Vec3f& point, float &d) const {
    Vec3f dir = b - a;
    Vec3f::value_type len = (point - a).Dot(dir);
    d = math::Clamp(len / dir.MagnitudeSq(), (Vec3f::value_type)0, (Vec3f::value_type)1);
    return a + dir * d;
}

Vec3f LineSegment::ClosestPoint(const Vec3f& point) const {
    float d;
    return ClosestPoint(point, d);
}

Vec3f LineSegment::ClosestPoint(const Line& line) const {
    float d1, d2;
    Line::ClosestPointLineLine(a, b - a, line.point, line.dir, d1, d2);

    if (d1 <= 0.0f) {
        return a;
    } else if (d1 >= 1.0f) {
        return b;
    } else {
        return a + (b - a) * d1;
    }
}

Vec3f LineSegment::ClosestPoint(const Ray& ray) const {
    float d1, d2;
    Line::ClosestPointLineLine(a, b - a, ray.origin, ray.direction, d1, d2);

    if (d1 <= 0.0f) {
        return a;
    } else if (d1 >= 1.0f) {
        return b;
    } else {
        return a + (b - a) * d1;
    }
}

Vec3f::value_type LineSegment::Distance(const Vec3f& point) const {
    Vec3f closestPoint = ClosestPoint(point);
    return closestPoint.Distance(point);
}

Vec3f::value_type LineSegment::DistanceSq(const Vec3f& point) const {
    Vec3f closestPoint = ClosestPoint(point);
    return closestPoint.DistanceSq(point);
}

bool LineSegment::Contains(const Vec3f& point) const {
    return DistanceSq(point) <= math::kEpsilonSq;
}

bool LineSegment::Intersects(const Ray& ray, float max, float& t) const {
    float d1, d2;
    ClosestPointSegmentSegment(a, b, ray.origin, ray.direction * max, d1, d2);
    if (d2 < 0.0f || d2 > 1.0f) {
        t = d2 / max;
        return false;
    }

    t = d2 * max;
    return true;
}

// http://geomalgorithms.com/a07-_distance.html
void LineSegment::ClosestPointSegmentSegment(const Vec3f& p0, const Vec3f& p1, const Vec3f& q0, const Vec3f& q1, float& d1, float& d2) {
    Vec3f u = p1 - p0;
    Vec3f v = q1 - q0;
    Vec3f w0 = p0 - q0;

    Vec3f::value_type e = w0.Dot(v);
    Vec3f::value_type b = v.Dot(u);
    Vec3f::value_type c = v.Dot(v);
    Vec3f::value_type d = w0.Dot(u);
    Vec3f::value_type a = u.Dot(u);
    Vec3f::value_type denom = a * c - b * b;

    Vec3f::value_type sN, sD = denom;       // sc = sN / sD, default sD = D >= 0
    Vec3f::value_type tN, tD = denom;       // tc = tN / tD, default tD = D >= 0

    // compute the line parameters of the two closest points
    if (denom < math::kEpsilon) { // the lines are almost parallel
        sN = 0.0f;         // force using point P0 on segment S1
        sD = 1.0f;         // to prevent possible division by 0.0 later
        tN = e;
        tD = c;
    }
    else {                 // get the closest points on the infinite lines
        sN = (b * e - c * d);
        tN = (a * e - b * d);
        if (sN < 0.0f) {        // sc < 0 => the s=0 edge is visible
            sN = 0.0f;
            tN = e;
            tD = c;
        }
        else if (sN > sD) {  // sc > 1  => the s=1 edge is visible
            sN = sD;
            tN = e + b;
            tD = c;
        }
    }

    if (tN < 0.0f) {            // tc < 0 => the t=0 edge is visible
        tN = 0.0f;
        // recompute sc for this edge
        if (-d < 0.0f)
            sN = 0.0f;
        else if (-d > a)
            sN = sD;
        else {
            sN = -d;
            sD = a;
        }
    }
    else if (tN > tD) {      // tc > 1  => the t=1 edge is visible
        tN = tD;
        // recompute sc for this edge
        if ((-d + b) < 0.0f)
            sN = 0.0f;
        else if ((-d + b) > a)
            sN = sD;
        else {
            sN = (-d + b);
            sD = a;
        }
    }

    // finally do the division to get sc and tc
    d1 = (math::Abs(sN) < math::kEpsilon ? 0.0f : sN / sD);
    d2 = (math::Abs(tN) < math::kEpsilon ? 0.0f : tN / tD);
}


//https://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect
bool LineSegment2D::Intersects(const LineSegment2D& other, float* t) const {
    auto p0 = a;
    auto p1 = b;
    auto p2 = other.a;
    auto p3 = other.b;

    auto s10 = p1 - p0;
    auto s32 = p3 - p2;
    float denom = s10.x * s32.y - s32.x * s10.y;
    if (denom == 0)
        return false; // Collinear
    bool denomPositive = denom > 0;

    auto s02 = p0 - p2;
    float s_numer = s10.x * s02.y - s10.y * s02.x;
    if ((s_numer < 0) == denomPositive)
        return false; // No collision

    float t_numer = s32.x * s02.y - s32.y * s02.x;
    if ((t_numer < 0) == denomPositive)
        return false; // No collision

    if (((s_numer > denom) == denomPositive) || ((t_numer > denom) == denomPositive))
        return false; // No collision

    // Collision detected
    if (t) {
        *t = t_numer / denom;
    }

    //if (intersect_point) {
    //    *intersect_point = p0 + (t * s10);
    //}

    return true;
}

}

