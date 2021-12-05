#pragma once

#include "Physics/Collision/Narrowphase/MinkowskiSum.h"
#include "CollisionDetector.h"

namespace glacier {

class Collider;

namespace physics {

class Gjk : public CollisionDetector {
public:
    Gjk(int max_iteration);
    bool Intersect(Collider* a, Collider* b, Simplex& simplex) override;
    bool Intersect(const AABB& a, Collider* b, Simplex& simplex) override;

private:
    bool UpdateSimplex(Simplex& simplex, Vec3f& dir);

    int max_iteration_;// = 50;
};

}
}
