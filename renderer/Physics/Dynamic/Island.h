#pragma once

#include <vector>
#include <stack>
#include "physics/dynamic/rigidbody.h"
#include "common/list.h"

namespace glacier {
namespace physics {

class ContactSolver;
class ContactManifold;

class Island {
public:
    constexpr static float kLinearSleepSq = 0.01f;
    constexpr static float kAngularSleepSq = 2.0f / 180.0f * math::kPI;
    constexpr static float kWarmStartRatio = 0.2f;

    Island();
    void Step(ContactSolver* solver, const List<Rigidbody>& world_bodies, float delta_time);

private:
    std::vector<ContactManifold*> manifolds_;
    std::vector<Rigidbody*> bodies_;
    std::stack<Rigidbody*> stack_;

    uint32_t version_ = 0;
};

}
}
