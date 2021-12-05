#pragma once

#include <unordered_map>
#include <vector>
#include <stdint.h>
#include "ContactManifold.h"
#include "Physics/Collision/CollidePair.h"

namespace glacier {

class Collider;
class Gizmos;

namespace physics {

class ContactSolver {
public:
    ContactSolver(uint32_t maxIter=10);
    ~ContactSolver();

    void ClearContact(Collider* collider);

    void Step(std::vector<CollidePair> &collideList);
    void UpdateContact();
    void AddContactPoint(Collider* a, Collider* b, const ContactPoint& ci);
    void Solve(std::vector<ContactManifold*>& mfs, float warmstartRatio) const;
    ContactManifold* Find(uint64_t key) const;
    void inv_interval(float value) { inv_interval_ = value; }

    const std::unordered_map<uint64_t, ContactManifold>& GetManifold() const { return manifolds_; }

    void Clear();
    void OnDrawGizmos();

private:
    float inv_interval_;
    uint32_t max_iteration_;

    std::unordered_map<uint64_t, ContactManifold> manifolds_;
    std::vector<uint64_t> removes_;
};

}
}
