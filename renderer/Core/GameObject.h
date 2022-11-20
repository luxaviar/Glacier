#pragma once

#include <string>
#include <vector>
#include <functional>
#include "Math/Vec3.h"
#include "Math/Quat.h"
#include "Math/Mat4.h"
#include "component.h"
#include "transform.h"
#include "common/list.h"
#include "linkable.h"
#include "identifiable.h"
#include "Common/TypeTraits.h"
#include "objectmanager.h"

namespace glacier {

class Component;
class GameObjectManager;

enum class GameObjectTag : uint32_t {
    kNone = 0,
};

enum class GameObjectLayer : uint32_t {
    kHide = 1,
};

class GameObject : 
    public Uncopyable,
    public Identifiable<GameObject>,
    public Linkable<GameObjectManager, GameObject>
{
public:
    friend class GameObjectManager;
    friend class Transform;

    static constexpr uint32_t kAllLayers = 0xFFFFFFFF;

    static GameObject& Create(const char* name = nullptr, GameObject* parent = nullptr);
    static void Destroy(GameObject* go);

    ~GameObject();

    template<typename T>
    T* GetComponent() {
        for (auto& com : components_) {
            auto ptr = com.get();
            T* ret = dynamic_cast<T*>(ptr);
            if (ret != nullptr) {
                return ret;
            }
        }
        return nullptr;
    }

    template<typename T, typename ...Args>
    T* AddComponent(Args&&... args) {
        assert(GetComponent<T>() == nullptr);

        auto com = std::make_unique<T>(std::forward<Args>(args)...);
        auto ret = com.get();
        com->game_object_ = this;

        ///TODO: Awake here?
        components_.emplace_back(std::move(com));
        
        ret->OnAwake();

        if (IsActive()) {
            ret->OnEnable();
        }
        return ret;
    }

    void AddComponentPtr(Component* com) {
        assert(com->game_object_ == nullptr);
        com->game_object_ = this;
        components_.emplace_back(std::move(com));

        com->OnAwake();

        if (IsActive()) {
            com->OnEnable();
        }
    }

    template<typename T>
    T* GetComponentInParent() {
        auto tx = transform_.parent();
        if (!tx) {
            return nullptr;
        }
        
        auto ret = tx->game_object()->GetComponent<T>();
        if (ret) {
            return ret;
        }

        return tx->game_object()->GetComponentInParent<T>();
    }

    template<typename T>
    void VisitComponents(const std::function<void(T*)>& callback) {
        for (auto& com : components_) {
            auto ptr = com.get();
            T* co = dynamic_cast<T*>(ptr);
            if (co != nullptr) {
                callback(co);
            }
        }
    }

    template<typename T>
    void VisitComponentsInChildren(const std::function<void(T*)>& callback) {
        auto children = transform_.children();
        for (auto ch : children) {
            auto go = ch->game_object();
            go->VisitComponents<T>(callback);
            go->VisitComponentsInChildren(callback);
        }
    }

    bool IsActive() const { return local_active_ && parent_active_ && !dying_; }
    bool IsDying() const { return dying_; }
    bool IsHidden() const { return (layer_ & toUType(GameObjectLayer::kHide)) != 0; }
    bool IsStatic() const { return static_; }

    void Activate();
    void Deactivate();
    void Hide();
    void ToggleStatic(bool on, bool recursively);

    const std::string& name() const { return name_; }
    void name(const char* name) { name_ = name; }

    Transform& transform() { return transform_; }
    const Transform& transform() const { return transform_; }

    uint32_t layer() const { return layer_; }
    void layer(GameObjectLayer v) { layer_ |= toUType(v); }

    uint32_t tag() const { return tag_; }
    void tag(GameObjectTag v) { tag_ = toUType(v); }

    GameObject* parent();
    const GameObject* parent() const;

    void DontDestroyOnLoad(bool v) { dont_destroy_onload_ = v; }
    bool DontDestroyOnLoad() const { return dont_destroy_onload_; }

    size_t ChildrenCount() const { return transform_.children().size(); }
    GameObject* GetChild(size_t idx);

    void DrawGizmos();
    void DrawInspector();
    void DrawSceneNode(uint32_t& selected);
    void DrawSelectedGizmos();

private:
    GameObject(const char* name = nullptr, GameObject* parent = nullptr);

    void SetParent(GameObject* parent);
    void OnParentChagne();

    void ActivateByParent();
    void DeactivateByParent();
    void DestryoRecursively();

    void EnableComponent();
    void DisableComponent();
    void DestroyComponent();

    bool dying_ = false;
    bool dont_destroy_onload_ = false;
    bool is_scene_node_ = false;

    bool local_active_ = true;
    bool parent_active_ = true;

    bool static_ = false;

    uint32_t tag_;
    uint32_t layer_;

    std::string name_;
    Transform transform_;
    std::vector<std::unique_ptr<Component>> components_;
};

class GameObjectManager : public BaseManager<GameObjectManager, GameObject> {
public:
    void AddSceneNode(GameObject* o);
    void RemoveSceneNode(GameObject* o);

    void DrawGizmos();

    void Destroy(GameObject* o);
    void CleanDead();

    void OnExit();
    void Clear(bool onload);

    const std::vector<GameObject*> GetSceneNodeList() const { return scene_node_list_; }

private:
    std::vector<GameObject*> scene_node_list_;
    std::vector<GameObject*> dying_list_;
};

}
