#include "ContactPoint.h"

namespace glacier {
namespace physics {

ContactInfo::ContactInfo(bool first_, Collider* other_, const Vec3f& point_, const Vec3f& point_other_,
    const Vec3f& normal_, float penetration_) : 
    first(first_),
    other(other_),
    point(point_),
    point_other(point_other_),
    normal(normal_),
    penetration(penetration_)
{
}

}
}
