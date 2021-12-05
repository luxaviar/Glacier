#pragma once

//#include "unico/lucid/klass.h"
#include "Common/Uncopyable.h"
#include "MinkowskiSum.h"
//#include "unico/geometry/shape.h"

namespace glacier {

class Collider;

namespace physics {

struct ContactPoint;

class NarrowPhaseDetector : private Uncopyable {
public:
    virtual ~NarrowPhaseDetector() {}
    virtual bool Detect(Collider* a, Collider* b, Simplex& simplex) = 0;
    virtual bool Detect(const AABB& a, Collider* b, Simplex& simplex) = 0;
    virtual bool Solve(Collider* a, Collider* b, Simplex& simplex, ContactPoint& ci) = 0;
    virtual void Clear() = 0;
};

}
}
