#include "Island.h"
#include <algorithm>
#include "ContactSolver.h"
#include "Physics/Collider/Collider.h"

namespace glacier {
namespace physics {

Island::Island() {
}

void Island::Step(ContactSolver* solver, const List<Rigidbody>& world_bodies, float delta_time) {
    ++version_;
    if (version_ == 0) version_ = 1;

    for (auto& seed : world_bodies) {
        if (seed->asleep() || seed->IsOnIsland(version_)) continue;

        manifolds_.clear();
        bodies_.clear();
        //stack_.clear();

        seed->AddIsland(version_);
        seed->Awake(true);

        stack_.push(seed.data);
        bodies_.push_back(seed.data);

        while (!stack_.empty()) {
            Rigidbody* body = stack_.top();
            stack_.pop();

            for (auto collider : body->colliders_) {
                for (auto key : collider->contacts_) {
                    ContactManifold* mf = solver->Find(key);
                    assert(mf);

                    if (!mf->IsOnIsland(version_)) {
                        manifolds_.push_back(mf);
                        mf->AddIsland(version_);
                    }

                    Collider* other_collider = mf->GetOther(collider);
                    if (!other_collider) continue;

                    Rigidbody* other = other_collider->rigidbody();
                    if (!other || other->IsOnIsland(version_)) continue;

                    other->AddIsland(version_);
                    other->Awake(true);

                    stack_.push(other);
                    bodies_.push_back(other);
                }
            }
        }

        solver->Solve(manifolds_, kWarmStartRatio);

        float minSleepTime = (std::numeric_limits<float>::max)();
        for (auto body : bodies_) {
            if (body->linear_velocity().MagnitudeSq() > kLinearSleepSq ||
                body->angular_velocity().MagnitudeSq() > kAngularSleepSq) {
                body->Awake();
                minSleepTime = 0.0f;
            } else {
                body->Sleep(delta_time);
                minSleepTime = math::Min(minSleepTime, body->sleep_time());
            }
        }

        if (minSleepTime > 0.5f) {
            for (auto body : bodies_) {
                if (body->is_dynamic()) {
                    body->SetAsleep();
                }
            }
        }
    }

    manifolds_.clear();
    bodies_.clear();
    //stack_.Clear();
}

}
}

