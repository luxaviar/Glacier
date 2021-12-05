#pragma once

#include "Physics/Collision/Narrowphase/NarrowphaseDetector.h"
//#include "unico/geometry/shape.h"
#include "Physics/Collider/Collider.h"

namespace glacier {
namespace physics {

class CollisionDetector;
class ContactManifoldSolver;
struct Simplex;
struct ContactPoint;

class ContactDetector : public NarrowPhaseDetector {
public:
    ContactDetector(std::unique_ptr<CollisionDetector>&& detector,
        std::unique_ptr<ContactManifoldSolver>&& generator);
    ~ContactDetector();

    bool Detect(Collider* a, Collider* b, Simplex& simplex) override;
    bool Detect(const AABB& a, Collider* b, Simplex& simplex) override;
    bool Solve(Collider* a, Collider* b, Simplex& simplex, ContactPoint& ci) override;
    void Clear() override;

private:
    std::unique_ptr<CollisionDetector> collision_detector_;
    std::unique_ptr<ContactManifoldSolver> contact_generator_;
};

}
}
