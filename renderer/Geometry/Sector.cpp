#include "sector.h"
#include "Math/Quat.h"
#include "Math/Vec3.h"

namespace glacier {

Sector::Sector(const Vec2f& center_, const Vec2f& dir_, float radius_, float half_angle_) :
    center(center_),
    radius(radius_),
    half_angle(half_angle_)
{
    Vec3f dir(dir_.x, 0, dir_.y);
    dir.Normalize();
    dir.Scale(radius_);

    Vec3f right_v3 = Quaternion::AngleAxis(half_angle_, Vec3f::up) * dir;
    Vec3f left_v3 = Quaternion::AngleAxis(-half_angle_, Vec3f::up) * dir;

    right.Set(right_v3.x, right_v3.z);
    left.Set(left_v3.x, left_v3.z);
}

bool Sector::Contains(const Vec2f& point) const {
    Vec2f vec = point - center;
    if (vec.MagnitudeSq() > radius * radius) {
        return false;
    }

    if (vec.Cross(left) < 0) {
        return false;
    }

    if (vec.Cross(right) > 0) {
        return false;
    }

    return true;
}

bool Sector::Contains(const Vec3f& point) const {
    Vec2f p(point.x, point.z);
    return Contains(p);
}

}

