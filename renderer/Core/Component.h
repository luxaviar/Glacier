#pragma once

#include <string>
#include "Math/Vec3.h"
#include "Math/Quat.h"
#include "Math/Mat4.h"
#include "Common/Uncopyable.h"

namespace glacier {

class GameObject;
class Transform;

class Component : private Uncopyable {
public:
    friend class GameObject;

    Component() noexcept {};
    virtual ~Component() {}

    Transform& transform();
    const Transform& transform() const;

    GameObject* game_object() { return game_object_; }
    const GameObject* game_object() const { return game_object_; }

    void Enable();
    void Disable(bool force=false);

    bool IsEnable() const { return enable_; }

    bool IsActive() const;
    bool IsHidden() const { return hide_; }

    void Hide() { hide_ = true; }

    virtual void Awake() {}
    
    uint32_t layer() const;
    uint32_t tag() const;

    virtual void OnAwake() {}
    virtual void OnEnable() {}
    virtual void OnDisable() {}
    virtual void OnDestroy() {}

    virtual void OnParentChange() {}

    virtual void DrawInspector() {}
    virtual void OnDrawGizmos() {}
    virtual void OnDrawSelectedGizmos() {}

protected:
    bool enable_ = true;
    bool hide_ = false;
    GameObject* game_object_ = nullptr;
};

//template<typename T>
//class BaseComponent : public Component {
//public:
//    friend class GameObject;
//    BaseComponent() noexcept {
//        id_ = &kTypeID;
//    }
//
//protected:
//    static const void* kTypeID;
//};
//
//template<typename T>
//const void* BaseComponent<T>::kTypeID = nullptr;

//template<typename T>
//class Identificable {
//public:
//    const void* type_id() { return type_; }
//
//protected:
//    static const T* const type_ = nullptr;
//};

}
