#pragma once

#include <unordered_map>
#include "Common/Uncopyable.h"
#include "Common/Singleton.h"

namespace glacier {

namespace render {
class Camera;
class Renderer;
}

enum class SceneLoadMode : uint8_t {
    kSingle,
    kAdditive
};

class Scene : private Uncopyable {  
public:
    Scene(const char* name) noexcept;

    const std::string& name() const { return name_; }

    virtual void OnLoad(render::Renderer* renderer) {}
    virtual void OnUnload(render::Renderer* renderer) {}

    render::Camera* GetMainCamera() { return main_camera_; }

protected:
    std::string name_;
    render::Camera* main_camera_ = nullptr;
};

class SceneManager : public Singleton<SceneManager> {
public:
    void Load(const char* name, SceneLoadMode mode);
    void Add(std::unique_ptr<Scene>&& scene);

    void Update(render::Renderer* renderer);

    Scene* CurrentScene() { return cur_scene_; }

private:
    Scene* cur_scene_ = nullptr;
    Scene* next_scene_ = nullptr;
    std::unordered_map<std::string, std::unique_ptr<Scene>> scenes_;
};

}
