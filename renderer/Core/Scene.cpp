#include "scene.h"
#include "gameobject.h"
#include "physics/world.h"
#include "Common/MoveWrapper.h"
#include "App.h"

namespace glacier {

LUX_CONSTANT_MULTI(SceneLoadMode, SceneLoadMode)
LUX_CONST("kSingle", SceneLoadMode::kSingle)
LUX_CONST("kAdditive", SceneLoadMode::kAdditive)
LUX_CONSTANT_MULTI_END

LUX_IMPL(Scene, Scene)
LUX_CTOR(Scene, const char*)
LUX_FUNC(Scene, SetMainCamera)
LUX_FUNC_SPEC(Scene, OnLoad, OnLoad, void, const lux::function&)
LUX_FUNC_SPEC(Scene, OnUnload, OnUnload, void, const lux::function&)
LUX_IMPL_END

Scene::Scene(const char* name) noexcept : name_(name) {
}

void Scene::OnLoad(OnLoadCallback&& callback) {
    on_load_ = std::move(callback);
}

void Scene::OnUnload(OnUnloadCallback&& callback) {
    on_unload_ = std::move(callback);
}

void Scene::OnLoad(const lux::function& fn) {
    auto callback = move_wrapper(lux::refable(fn));
    on_load_ = [this, callback](render::Renderer* renderer) {
        App::Self()->VM().CallRef(*callback, renderer);
    };
}

void Scene::OnUnload(const lux::function& fn) {
    auto callback = move_wrapper(lux::refable(fn));
    on_unload_ = [this, callback](render::Renderer* renderer) {
        App::Self()->VM().CallRef(*callback, renderer);
    };
}

LUX_IMPL(SceneManager, SceneManager)
LUX_CTOR(SceneManager)
LUX_FUNC(SceneManager, Instance)
LUX_FUNC(SceneManager, Add)
LUX_FUNC(SceneManager, Load)
LUX_IMPL_END

void SceneManager::Update(render::Renderer* renderer) {
    if (!next_scene_) return;

    auto target_scene = next_scene_;
    next_scene_ = nullptr;

    if (cur_scene_) {
        //cur_scene_->OnUnload(renderer);
        if (cur_scene_->on_unload_) {
            cur_scene_->on_unload_(renderer);
        }
        else {
            cur_scene_->OnUnload(renderer);
        }
        cur_scene_ = nullptr;
    }

    physics::World::Instance()->Clear();
    GameObjectManager::Instance()->Clear(true);
    
    cur_scene_ = target_scene;
    //cur_scene_->OnLoad(renderer);
    if (cur_scene_->on_load_) {
        cur_scene_->on_load_(renderer);
    }
    else {
        cur_scene_->OnLoad(renderer);
    }
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

void SceneManager::ClearAll() {
    scenes_.clear();
}

}
