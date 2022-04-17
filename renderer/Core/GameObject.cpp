#include "GameObject.h"
#include "imgui/imgui.h"
#include "Component.h"
#include "Math/Util.h"
#include "Transform.h"
#include "Render/Mesh/MeshRenderer.h"

namespace glacier {

GameObject& GameObject::Create(const char* name, GameObject* parent) {
    GameObject* go = new GameObject(name, parent);

    return *go;
}

void GameObject::Destroy(GameObject* go) {
    GameObjectManager::Instance()->Destroy(go);
}

GameObject::GameObject(const char* name, GameObject* parent) : 
    name_(name ? name : "GameObject") 
{
    transform_.game_object_ = this;

    if (!parent) {
        GameObjectManager::Instance()->AddSceneNode(this);
    }
    else {
        transform_.SetParent(&parent->transform_);
    }
}

GameObject::~GameObject() {
    if (is_scene_node_) {
        GameObjectManager::Instance()->RemoveSceneNode(this);
    }
}

void GameObject::Hide() {
    layer(GameObjectLayer::kHide);
}

void GameObject::ToggleStatic(bool on, bool recursively) {
    if (dying_) return;

    if (static_ != on) {
        for (auto& c : components_) {
            if (on) {
                c->OnStatic();
            }
            else {
                c->OnNotStatic();
            }
        }
        static_ = on;
    }

    if (!recursively) return;

    auto children = transform_.children();
    for (auto ch : children) {
        ch->game_object()->ToggleStatic(on, recursively);
    }
}

GameObject* GameObject::parent() {
    if (!transform_.parent()) return nullptr;
    return transform_.parent()->game_object();
}

const GameObject* GameObject::parent() const {
    if (!transform_.parent()) return nullptr;
    return transform_.parent()->game_object();
}

GameObject* GameObject::GetChild(size_t idx) {
    if (idx >= transform_.children().size()) return nullptr;

    auto tx = transform_.children()[idx];
    return tx->game_object();
}

void GameObject::SetParent(GameObject* parent) {
    if (dying_) return;

    auto old_active = IsActive();
    if (!parent) {
        parent_active_ = true;
        if (!is_scene_node_) {
            GameObjectManager::Instance()->AddSceneNode(this);
        }
    }
    else {
        parent_active_ = parent->IsActive();
        if (is_scene_node_) {
            GameObjectManager::Instance()->RemoveSceneNode(this);
        }
    }

    OnParentChagne();

    auto cur_active = IsActive();
    if (cur_active == old_active) {
        return;
    }

    if (cur_active) {
        ActivateByParent();
    } else {
        DeactivateByParent();
    }
}

void GameObject::OnParentChagne() {
    for (auto& c : components_) {
        c->OnParentChange();
    }

    auto children = transform_.children();
    for (auto ch : children) {
        ch->game_object()->OnParentChagne();
    }
}

void GameObject::Activate() {
    if (dying_ || local_active_) return;

    local_active_ = true;
    if (!parent_active_) return;

    EnableComponent();

    auto children = transform_.children();
    for (auto ch : children) {
        ch->game_object()->ActivateByParent();
    }
}

void GameObject::Deactivate() {
    if (dying_ || !local_active_) return;

    local_active_ = false;
    if (!parent_active_) return;
    
    DisableComponent();

    auto children = transform_.children();
    for (auto ch : children) {
        ch->game_object()->DeactivateByParent();
    }
}

void GameObject::ActivateByParent() {
    if (dying_ || parent_active_) return;

    parent_active_ = true;
    if (local_active_) {
        EnableComponent();
    }

    auto children = transform_.children();
    for (auto ch : children) {
        ch->game_object()->ActivateByParent();
    }
}

void GameObject::DeactivateByParent() {
    if (dying_ || !parent_active_) return;

    parent_active_ = false;
    if (local_active_) {
        DisableComponent();
    }

    auto children = transform_.children();
    for (auto ch : children) {
        ch->game_object()->DeactivateByParent();
    }
}

void GameObject::DestryoRecursively() {
    if (dying_) return;

    dying_ = true;
    //auto cur_active = IsActive();
    //if (cur_active) {
        //DestroyComponent();
    //}
    DestroyComponent();

    auto children = transform_.children();
    for (auto ch : children) {
        Destroy(ch->game_object());
    }
}

void GameObject::EnableComponent() {
    for (auto& c : components_) {
        if (c->IsEnable()) {
            c->OnEnable();
        }
    }
}

void GameObject::DisableComponent() {
    for (auto& c : components_) {
        if (c->IsEnable()) {
            c->OnDisable();
        }
    }
}

void GameObject::DestroyComponent() {
    bool active = local_active_ && parent_active_;
    for (auto& c : components_) {
        if (active) {
            c->Disable(true);
        }
        c->OnDestroy();
    }
}

void GameObject::DrawGizmos() {
    if (!IsActive()) return;

    //transform_.OnDrawGizmos();
    for (auto& c : components_) {
        if (c->IsEnable() && !c->IsHidden()) {
            c->OnDrawGizmos();
        }
    }

    auto& children = transform_.children();
    for (auto& ch : children) {
        auto go = ch->game_object();
        if (go) {
            go->DrawGizmos();
        }
    }
}

void GameObject::DrawInspector() {
    transform_.DrawInspector();
    for (auto& c : components_) {
        if (!c->IsHidden()) {
            c->DrawInspector();
        }
    }
}

void GameObject::DrawSelectedGizmos() {
    transform_.OnDrawSelectedGizmos();
    for (auto& c : components_) {
        if (!c->IsHidden()) {
            c->OnDrawSelectedGizmos();
        }
    }
}

void GameObject::DrawSceneNode(uint32_t& selected) {
    if (dying_ || IsHidden()) return;

    ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_OpenOnDoubleClick |
        (transform_.children().empty() ? ImGuiTreeNodeFlags_Leaf : 0) |
        ((selected == id_) ? ImGuiTreeNodeFlags_Selected : 0);

    if (!IsActive()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    }

    bool open = ImGui::TreeNodeEx(this, node_flags, name_.c_str());

    if (!IsActive()) {
        ImGui::PopStyleColor(1);
    }

    bool clicked = false;
    if (ImGui::IsItemClicked()) {
        selected = id_;
    }

    if (open) {
        for (auto& ch : transform_.children()) {
            ch->game_object()->DrawSceneNode(selected);
        }
        ImGui::TreePop();
    }
}

void GameObjectManager::AddSceneNode(GameObject* o) {
    assert(!o->is_scene_node_);

    scene_node_list_.push_back(o);
    o->is_scene_node_ = true;
}

void GameObjectManager::RemoveSceneNode(GameObject* o) {
    assert(o->is_scene_node_);

    auto it = std::find(scene_node_list_.begin(), scene_node_list_.end(), o);
    if (it != scene_node_list_.end()) {
        scene_node_list_.erase(it);
        o->is_scene_node_ = false;
    }
}

void GameObjectManager::DrawGizmos() {
    for (auto go : scene_node_list_) {
        go->DrawGizmos();
    }
}

void GameObjectManager::Destroy(GameObject* o) {
    //assert(!o->dying_);
    if (o->dying_) return;

    //delete children first
    o->DestryoRecursively();

    dying_list_.push_back(o);

    if (o->is_scene_node_) {
        RemoveSceneNode(o);
    }
}

void GameObjectManager::CleanDead() {
    if (dying_list_.empty()) return;

    for (auto o : dying_list_) {
        o->node_.RemoveFromList();
        delete o;
    }

    dying_list_.clear();
}

void GameObjectManager::Clear(bool onload) {
    CleanDead();

    if (scene_node_list_.empty()) return;

    for (size_t i = scene_node_list_.size(); i > 0; --i) {
        auto o = scene_node_list_[i - 1];
        if (onload && o->dont_destroy_onload_) continue;

        Destroy(o);
    }

    for (auto o : dying_list_) {
        delete o;
    }
}

void GameObjectManager::OnExit() {
    Clear(false);
}

}
