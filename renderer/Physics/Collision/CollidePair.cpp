#include "CollidePair.h"
#include "Physics/Collider/Collider.h"

namespace glacier {
namespace physics {

CollidePair::CollidePair(Collider* first_, Collider* second_) {
    if (first_->id() > second_->id()) {
        Collider* tmp = first_;
        first_ = second_;
        second_ = tmp;
    }

    first = first_;
    second = second_;
    sensor = true;
}

CollidePair::CollidePair(Collider* first_, Collider* second_, const ContactPoint& contact_, bool sensor_) {
    if (first_->id() > second_->id()) {
        Collider* tmp = first_;
        first_ = second_;
        second_ = tmp;
    }

    first = first_;
    second = second_;
    contact = contact_;
    sensor = sensor_;
}

bool CollidePair::Equals(const CollidePair& other) const {
    return first->id() == other.first->id() && second->id() == other.second->id();
}

}
}

