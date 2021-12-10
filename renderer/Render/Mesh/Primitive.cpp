#include "primitive.h"
#include "Core/GameObject.h"
#include "MeshRenderer.h"

namespace glacier {
namespace render {

GameObject& Primitive::CreateCube(Material* material, const Vec3f size) {
    VertexCollection vertices;
    IndexCollection indices;

    geometry::CreateCube(vertices, indices, size);

    auto mesh = std::make_shared<Mesh>(vertices, indices);
    auto& go = GameObject::Create("cube");
    go.AddComponent<MeshRenderer>(mesh, material);

    return go;
}

GameObject& Primitive::CreateSphere(Material* material, float radius) {
    VertexCollection vertices;
    IndexCollection indices;

    geometry::CreateSphere(vertices, indices, radius);

    auto mesh = std::make_shared<Mesh>(vertices, indices);
    auto& go = GameObject::Create("sphere");
    go.AddComponent<MeshRenderer>(mesh, material);

    return go;
}

GameObject& Primitive::CreateCylinder(Material* material, float radius, float height) {
    VertexCollection vertices;
    IndexCollection indices;

    geometry::CreateCylinder(vertices, indices, height, radius);

    auto mesh = std::make_shared<Mesh>(vertices, indices);
    auto& go = GameObject::Create("cylinder");
    go.AddComponent<MeshRenderer>(mesh, material);

    return go;
}

GameObject& Primitive::CreateCapsule(Material* material, float radius, float mid_height) {
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
