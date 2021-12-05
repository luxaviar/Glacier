#pragma once

#include <memory>
#include "core/scene.h"

namespace glacier {
namespace render {

class PhysicsDemo : public Scene {
public:
    PhysicsDemo(const char* name) : Scene(name) {}
    void OnLoad(Renderer* renderer) override;
};

}
}
