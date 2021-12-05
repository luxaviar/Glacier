#include "scene.h"
#include "gameobject.h"
#include "physics/world.h"

namespace glacier {

Scene::Scene(const char* name) noexcept : name_(name) {
}

void SceneManager::Update(render::Renderer* renderer) {
    if (!next_scene_) return;

    auto target_scene = next_scene_;
    next_scene_ = nullptr;

    if (cur_scene_) {
        cur_scene_->OnUnload(renderer);
        cur_scene_ = nullptr;
    }

    physics::World::Instance()->Clear();
    GameObjectManager::Instance()->Clear(true);
    
    cur_scene_ = target_scene;
    cur_scene_->OnLoad(renderer);
}

void SceneManager::Load(const char* name, SceneLoadMode mode) {
    auto it = scenes_.find(name);
    if (it == scenes_.end()) {
        //TODO: log
        return;
    }

    //if (!cur_scene_) {
    //    cur_scene_ = it->second.get();
    //    cur_scene_->OnLoad();
    //    return;
    //}

    next_scene_ = it->second.get();
}

void SceneManager::Add(std::unique_ptr<Scene>&& scene) {
    auto it = scenes_.find(scene->name());
    if (it != scenes_.end()) {
        //TODO: log
        return;
    }

    scenes_.emplace(scene->name(), std::move(scene));
}

}
