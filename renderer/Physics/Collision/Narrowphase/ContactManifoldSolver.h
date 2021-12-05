#pragma once

#include "Common/Uncopyable.h"
//#include "unico/geometry/shape.h"

namespace glacier {

class Collider;

namespace physics {

struct ContactPoint;
struct Simplex;

class ContactManifoldSolver : private Uncopyable {
public:
    virtual ~ContactManifoldSolver() {};
    virtual bool Solve(Collider* a, Collider* b, Simplex& simplex, ContactPoint& ci) = 0;
};

}
}
