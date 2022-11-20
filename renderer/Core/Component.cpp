#include "component.h"
#include "Math/Util.h"
#include "transform.h"
#include "gameobject.h"
#include "Lux/Lux.h"

namespace glacier {

LUX_IMPL(Component, Component)
//LUX_CTOR(Component, const std::string&)
//LUX_FUNC(Component, Setup)
LUX_IMPL_END

Transform& Component::transform() {
    return game_object_->transform();
}

const Transform& Component::transform() const {
    return game_object_->transform();
}

bool Component::IsActive() const {
    return enable_ && game_object_->IsActive();
}

bool Component::IsHidden() const {
    return hide_ || game_object_->IsHidden();
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
