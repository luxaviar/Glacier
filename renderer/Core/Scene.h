#pragma once

#include <unordered_map>
#include "Common/Uncopyable.h"
#include "Common/Singleton.h"
#include "Lux/Wrapper.h"

namespace glacier {

namespace render {
class Camera;
class Renderer;
class Renderable;
}

enum class SceneLoadMode : uint8_t {
    kSingle,
    kAdditive
};

class Scene : private Uncopyable {  
public:
    friend class SceneManager;

    using OnLoadCallback = std::function<void(render::Renderer* renderer)>;
    using OnUnloadCallback = std::function<void(render::Renderer* renderer)>;

    Scene(const char* name) noexcept;

    const std::string& name() const { return name_; }

    virtual void OnLoad(render::Renderer* renderer) {}
    virtual void OnUnload(render::Renderer* renderer) {}

    void OnLoad(OnLoadCallback&& callback);
    void OnUnload(OnUnloadCallback&& callback);

    void OnLoad(const lux::function& fn);
    void OnUnload(const lux::function& fn);

    render::Camera* GetMainCamera() { return main_camera_; }
    void SetMainCamera(render::Camera* camera) {
        main_camera_ = camera;
    }

protected:
    std::string name_;
    render::Camera* main_camera_ = nullptr;

    OnLoadCallback on_load_;
    OnUnloadCallback on_unload_;
};

class SceneManager : public Singleton<SceneManager> {
public:
    void Load(const char* name, SceneLoadMode mode);
    void Add(std::unique_ptr<Scene>&& scene);

    void Update(render::Renderer* renderer);

    Scene* CurrentScene() { return cur_scene_; }
    void ClearAll();

private:
    Scene* cur_scene_ = nullptr;
    Scene* next_scene_ = nullptr;
    std::unordered_map<std::string, std::unique_ptr<Scene>> scenes_;
};

}
