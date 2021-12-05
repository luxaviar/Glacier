#pragma once

#include "Physics/Collision/CollidePair.h"
#include "Physics/Collision/ContactPoint.h"

namespace glacier {

class Collider;

namespace physics {

//class Fixture;

struct CollidePair {
    Collider* first;
    Collider* second;
    ContactPoint contact;
    bool sensor;

    CollidePair(Collider* first_, Collider* second_);
    CollidePair(Collider* first_, Collider* second_, const ContactPoint& contact_, bool sensor_ = true);
    bool Equals(const CollidePair& other) const;
};

}
}

