#include "PbrScene.h"
#include <memory>
#include <assert.h>
#include <algorithm>
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui.h"
#include "Math/Util.h"
#include "Render/Graph/PassNode.h"
#include "Render/Graph/ResourceEntry.h"
#include "Render/Mesh/model.h"
#include "render/camera.h"
#include "Math/Vec3.h"
#include "Math/Mat4.h"
#include "Core/GameObject.h"
#include "Behaviour/CameraController.h"
#include "Render/Renderer.h"
#include "Render/Light.h"
#include "Render/Mesh/Primitive.h"

namespace glacier {
namespace render {

void PbrScene::OnLoad(Renderer* renderer) {
    auto gfx = renderer->driver();

    auto& camera_go = GameObject::Create("MainCamera");
    main_camera_ = camera_go.AddComponent<Camera>();
    main_camera_->position({ 11.0f,8.0f,-20.0f });
    main_camera_->LookAt({ -0.0f,0.0f,0.0f });
    main_camera_->farz(100.0f);

    camera_go.AddComponent<CameraController>(main_camera_);

    auto& light_go = GameObject::Create("main_light");
    auto dir = (Vec3f(1, -1, 0) - Vec3f::zero).Normalized();
    light_go.transform().forward(dir);

    auto main_light = light_go.AddComponent<DirectionalLight>(Color::kWhite, 1.0f);
    main_light->EnableShadow();

    float radius = 0.5f;
    auto solid_mat = MaterialManager::Instance()->Get("solid");
    auto& sphere_go = Primitive::CreateSphere(solid_mat, radius);
    sphere_go.name("light sphere");
    auto light = sphere_go.AddComponent<PointLight>(20.0f, Color::kWhite, 1.0f);
    sphere_go.transform().position(light->position());
    sphere_go.GetComponent<MeshRenderer>()->SetCastShadow(false);

    auto pbr_default = MaterialManager::Instance()->Get("pbr_default");
    auto size = Vec3f{ 4.0f, 4.0f, 4.0f };
    auto& cube_go = Primitive::CreateCube(pbr_default, size);
    cube_go.transform().position({-6.0f, 0.0f, 0.0f});

    auto& cube1_go = Primitive::CreateCube(pbr_default, size);
    cube1_go.transform().position({ -12.0f,4.0f,0.0f });

    auto& cube2_go = Primitive::CreateCube(pbr_default, size);
    cube2_go.transform().position({ -6.0f,0.0f,20.0f });

    auto& cube3_go = Primitive::CreateCube(pbr_default, size);
    cube3_go.transform().position({ -12.0f,4.0f,20.0f });

    auto helmet_mat = MaterialManager::Instance()->Get("pbr_helmet");
    auto& helmet = Model::GenerateGameObject("assets\\model\\helmet\\helmet.obj",
        helmet_mat, 2.0f);

    helmet.transform().position({ 6.0f,4.0f,10.0f });
}

}
}
