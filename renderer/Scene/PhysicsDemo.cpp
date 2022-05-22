#include "PhysicsDemo.h"
#include <memory>
#include <assert.h>
#include <algorithm>
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui.h"
#include "Math/Util.h"
#include "Render/Graph/PassNode.h"
#include "Render/Graph/ResourceEntry.h"
#include "Render/Mesh/Model.h"
#include "Render/Camera.h"
#include "Math/Vec3.h"
#include "Math/Mat4.h"
#include "Math/Random.h"
#include "Core/GameObject.h"
#include "Behaviour/CameraController.h"
#include "Behaviour/Fall.h"
#include "Render/Renderer.h"
#include "Render/Light.h"
#include "Render/Mesh/Primitive.h"
#include "Physics/Dynamic/Rigidbody.h"
#include "Physics/Collider/BoxCollider.h"

namespace glacier {
namespace render {

void PhysicsDemo::OnLoad(Renderer* renderer) {
    auto gfx = renderer->driver();

    auto& camera_go = GameObject::Create("MainCamera");
    main_camera_ = camera_go.AddComponent<Camera>();
    main_camera_->position({ 11.0f,8.0f,-20.0f });
    main_camera_->LookAt({ -0.0f,0.0f,0.0f });
    main_camera_->fov(45);

    camera_go.AddComponent<CameraController>(main_camera_);
    camera_go.AddComponent<Fall>();

    auto& light_go = GameObject::Create("main_light");
    auto dir = (Vec3f(1, -1, 0) - Vec3f::zero).Normalized();
    light_go.transform().forward(dir);

    auto main_light = light_go.AddComponent<DirectionalLight>(Color::kWhite, 1.0f);
    main_light->EnableShadow();

    auto pbr_floor = MaterialManager::Instance()->Get("pbr_floor");

    auto ground_size = Vec3f(50.0f, 2.0f, 50.0f);
    auto& ground_go = Primitive::CreateCube(pbr_floor, ground_size);
    ground_go.name("floor");
    ground_go.transform().position({ 0.0f,-1.5f,0.0f });
    ground_go.AddComponent<BoxCollider>(Vec3f{ 25.0f, 1.0f, 25.0f });

    auto helmet_mat = MaterialManager::Instance()->Get("pbr_helmet");
    auto& helmet = Model::GenerateGameObject("assets\\model\\helmet\\helmet.obj",
        helmet_mat, 2.0f);

    helmet.transform().position({ 7.5f,6.0f,-14.0f });
}

}
}
