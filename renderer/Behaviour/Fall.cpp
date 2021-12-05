#include "Fall.h"
#include "Math/Util.h"
#include "Math/Random.h"
#include "Render/Camera.h"
#include "Input/Input.h"
#include "Render/Material.h"
#include "Render/Mesh/Primitive.h"
#include "Core/GameObject.h"
#include "Physics/Dynamic/Rigidbody.h"
#include "Physics/Collider/BoxCollider.h"
#include "Physics/Collider/CylinderCollider.h"
#include "Physics/Collider/SphereCollider.h"
#include "Physics/Collider/CapsuleCollider.h"

namespace glacier {

void Fall::SpawnCube() {
    auto pbr_default = render::MaterialManager::Instance()->Get("pbr_default");
    auto size = Vec3f{ 1.0f, 1.0f, 1.0f };
    auto& cube_go = render::Primitive::CreateCube(pbr_default, size);
    cube_go.transform().position({ 0.0f, 5.0f, 0.0f });

    auto rigidbody = cube_go.AddComponent<Rigidbody>(RigidbodyType::kDynamic, true);
    auto collider = cube_go.AddComponent<BoxCollider>(Vec3f{ 0.5f, 0.5f, 0.5f });
    collider->mass(0.5f);
}

void Fall::SpawnSphere() {
    auto pbr_default = render::MaterialManager::Instance()->Get("pbr_default");
    float radius = 0.5f;
    auto& cube_go = render::Primitive::CreateSphere(pbr_default, radius);
    cube_go.transform().position({ 0.0f, 5.0f, 0.0f });

    auto rigidbody = cube_go.AddComponent<Rigidbody>(RigidbodyType::kDynamic, true);
    auto collider = cube_go.AddComponent<SphereCollider>(radius);
    collider->mass(0.5f);
}

void Fall::SpawnCylinder() {
    auto pbr_default = render::MaterialManager::Instance()->Get("pbr_default");
    float radius = 0.5f;
    float height = 1.0f;

    auto& cube_go = render::Primitive::CreateCylinder(pbr_default, height, radius);
    cube_go.transform().position({ 0.0f, 5.0f, 0.0f });
    cube_go.transform().rotation(Quaternion::AngleAxis(90, Vec3f::right));

    auto rigidbody = cube_go.AddComponent<Rigidbody>(RigidbodyType::kDynamic, true);
    auto collider = cube_go.AddComponent<CylinderCollider>(height, radius);
    collider->mass(0.5f);
}

void Fall::SpawnCapsule() {
    auto pbr_default = render::MaterialManager::Instance()->Get("pbr_default");
    float radius = 0.5f;
    float height = 1.0f;

    auto& cube_go = render::Primitive::CreateCapsule(pbr_default, height, radius);
    cube_go.transform().position({ 0.0f, 5.0f, 0.0f });

    auto rigidbody = cube_go.AddComponent<Rigidbody>(RigidbodyType::kDynamic, true);
    auto collider = cube_go.AddComponent<CapsuleCollider>(height, radius);
    collider->mass(0.5f);
}

void Fall::Update(float dt) {
    if (Input::IsRelativeMode()) return;

    if (Input::IsJustKeyDown(Keyboard::F)) {
        auto r = random::Range(1, 4);
        switch (r)
        {
        case 1:
            SpawnSphere();
            break;
        case 2:
            SpawnCapsule();
            break;
        case 3:
            SpawnCylinder();
            break;
        default:
            SpawnCube();
        }
    }
}

}