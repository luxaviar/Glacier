#include "ContactDetector.h"
#include "ContactManifoldSolver.h"
#include "CollisionDetector.h"

namespace glacier {
namespace physics {

ContactDetector::ContactDetector(std::unique_ptr<CollisionDetector>&& detector,
        std::unique_ptr<ContactManifoldSolver>&& generator) :
    collision_detector_(std::move(detector)),
    contact_generator_(std::move(generator))
{
}

ContactDetector::~ContactDetector() {

}

bool ContactDetector::Detect(Collider* a, Collider* b, Simplex& simplex) {
    return collision_detector_->Intersect(a, b, simplex);
}

bool ContactDetector::Detect(const AABB& a, Collider* b, Simplex& simplex) {
    return collision_detector_->Intersect(a, b, simplex);
}

bool ContactDetector::Solve(Collider* a, Collider* b, Simplex& simplex, ContactPoint& ci) {
    return contact_generator_->Solve(a, b, simplex, ci);
}

void ContactDetector::Clear() {

}

}
}

