#include "component.h"
#include "Math/Util.h"
#include "transform.h"
#include "gameobject.h"

namespace glacier {

Transform& Component::transform() {
    return game_object_->transform();
}

const Transform& Component::transform() const {
    return game_object_->transform();
}

bool Component::IsActive() const {
    return enable_ && game_object_->IsActive();
}

void Component::Enable() {
    if (enable_) return;

    auto old_active = IsActive();
    enable_ = true; 

    if (!old_active && IsActive()) {
        OnEnable();
    }
}

void Component::Disable(bool force) {
    if (!enable_) return;

    auto old_active = IsActive();
    enable_ = false;

    if (old_active || force) {
        OnDisable();
    }
}

uint32_t Component::layer() const {
    return game_object_->layer();
}

uint32_t Component::tag() const {
    return game_object_->tag();
}

//void Component::SetGameObject(GameObject* object) {
//    object_ = object;
//}

}
