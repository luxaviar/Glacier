#pragma once

#include "Common/Uncopyable.h"

namespace glacier {

class Collider;

namespace physics {

class CollisionFilter : private Uncopyable {
public:
    virtual ~CollisionFilter() {}
    virtual bool CanCollide(Collider* a, Collider* b) const = 0;
    virtual bool CanCollide(Collider* a) const = 0;
};

}
}

