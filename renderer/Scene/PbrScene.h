#pragma once

#include <memory>
#include "core/scene.h"

namespace glacier {
namespace render {

class GfxDriver;
class Material;

class PbrScene : public Scene {
public:
    PbrScene(const char* name) : Scene(name) {}
    void OnLoad(Renderer* renderer) override;
};

}
}
