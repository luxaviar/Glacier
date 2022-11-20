#include "primitive.h"
#include "Core/GameObject.h"
#include "MeshRenderer.h"
#include "Lux/Lux.h"

namespace glacier {
namespace render {

LUX_IMPL(Primitive, Primitive)
// LUX_CTOR(Material, const char*)
LUX_FUNC(Primitive, CreateCube)
LUX_FUNC(Primitive, CreateSphere)
LUX_FUNC(Primitive, CreateCylinder)
LUX_FUNC(Primitive, CreateCapsule)
LUX_IMPL_END

GameObject& Primitive::CreateCube(const std::shared_ptr<Material>& material, const Vec3f size) {
    VertexCollection vertices;
    IndexCollection indices;

    geometry::CreateCube(vertices, indices, size);

    auto mesh = std::make_shared<Mesh>(vertices, indices);
    auto& go = GameObject::Create("cube");
    go.AddComponent<MeshRenderer>(mesh, material);

    return go;
}

GameObject& Primitive::CreateSphere(const std::shared_ptr<Material>& material, float radius) {
    VertexCollection vertices;
    IndexCollection indices;

    geometry::CreateSphere(vertices, indices, radius);

    auto mesh = std::make_shared<Mesh>(vertices, indices);
    auto& go = GameObject::Create("sphere");
    go.AddComponent<MeshRenderer>(mesh, material);

    return go;
}

GameObject& Primitive::CreateCylinder(const std::shared_ptr<Material>& material, float radius, float height) {
    VertexCollection vertices;
    IndexCollection indices;

    geometry::CreateCylinder(vertices, indices, height, radius);

    auto mesh = std::make_shared<Mesh>(vertices, indices);
    auto& go = GameObject::Create("cylinder");
    go.AddComponent<MeshRenderer>(mesh, material);

    return go;
}

GameObject& Primitive::CreateCapsule(const std::shared_ptr<Material>& material, float radius, float mid_height) {
    VertexCollection vertices;
    IndexCollection indices;

    geometry::CreateCapsule(vertices, indices, mid_height, radius);

    auto mesh = std::make_shared<Mesh>(vertices, indices);
    auto& go = GameObject::Create("capsule");
    go.AddComponent<MeshRenderer>(mesh, material);

    return go;
}

}
}
