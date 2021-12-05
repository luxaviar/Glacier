#include "behaviour.h"

namespace glacier {

Behaviour::Behaviour() noexcept {
}

void BehaviourManager::Update(float dt) {
    for (auto it = objects_.begin(); it != objects_.end(); ++it) {
        auto be = it->data;
        if (be->IsActive()) {
            be->Update(dt);
        }
    }
}

void BehaviourManager::LateUpdate(float dt) {
    for (auto it = objects_.begin(); it != objects_.end(); ++it) {
        auto be = it->data;
        if (be->IsActive()) {
            be->LateUpdate(dt);
        }
    }
}

}
