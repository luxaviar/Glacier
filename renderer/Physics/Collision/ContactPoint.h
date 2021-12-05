#pragma once

#include "Math/Util.h"
#include "Math/Vec3.h"

namespace glacier {

class Collider;

namespace physics {

//all in world space
struct ContactPoint {
    Vec3f pointA; // point is on the surface of A (inside of B if penetration is positive)
    Vec3f pointB;
    Vec3f normal; // points from A to B
    float penetration;
};

struct ContactInfo {
    bool first; // is A?
    Collider* other;
    Vec3f point; // point is on the surface of self
    Vec3f point_other; // point is on the surface of other
    Vec3f normal; //points from self to other
    float penetration;

    ContactInfo(bool first_, Collider* other_, const Vec3f& point_, const Vec3f& point_other_,
        const Vec3f& normal_, float penetration_);
};

}
}