#pragma once

#include "Common/Uncopyable.h"
#include "geometry/aabb.h"

namespace glacier {

class Collider;

namespace physics {

struct Simplex;

class CollisionDetector : private Uncopyable {
public:
    virtual ~CollisionDetector() {};
    virtual bool Intersect(Collider* a, Collider* b, Simplex& sm) = 0;
    virtual bool Intersect(const AABB& a, Collider* b, Simplex& sm) = 0;
};

}
}
