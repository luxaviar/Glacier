#pragma once

#include "geometry.h"

namespace glacier {

class GameObject;

namespace render {

class Material;

class Primitive {
public:
    static GameObject& CreateCube(const std::shared_ptr<Material>& material, const Vec3f size = Vec3f::one);
    static GameObject& CreateSphere(const std::shared_ptr<Material>& material, float radius = 0.5f);
    static GameObject& CreateCylinder(const std::shared_ptr<Material>& material, float radius = 0.5f, float height = 1.0f);
    static GameObject& CreateCapsule(const std::shared_ptr<Material>& material, float radius = 0.5f, float mid_height = 1.0f);
};

}
}
