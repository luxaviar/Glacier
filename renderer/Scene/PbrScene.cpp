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
#include "Render/Base/CommandQueue.h"

namespace glacier {
namespace render {

static void DrawHelmet(CommandBuffer* cmd_buffer) {
    auto& helmet = Model::GenerateGameObject(cmd_buffer, "assets\\model\\helmet\\DamagedHelmet.gltf", true, 1.0f);
    //helmet.transform().position(Vector3{ 6.0f,4.0f,10.0f });
    helmet.transform().position(Vector3{ 99, 25, 4 });
}

static void DrawSponza(CommandBuffer* cmd_buffer) {
    auto& sponza = Model::GenerateGameObject(cmd_buffer, "assets\\model\\sponza\\sponza.gltf", true, 10.0f);
    //sponza.transform().position({ 6.0f,4.0f,10.0f });
}

void PbrScene::OnLoad(Renderer* renderer) {
    auto gfx = renderer->driver();

    auto& camera_go = GameObject::Create("MainCamera");
    main_camera_ = camera_go.AddComponent<Camera>();
    main_camera_->position({ 102.0f, 25.5f, 4.5f });

    auto rot = Quaternion::FromEuler(9.4f, 266.671f, 0);
    main_camera_->rotation(rot);

    camera_go.AddComponent<CameraController>(main_camera_);

    auto& light_go = GameObject::Create("main_light");
    auto dir = (Vec3f(1, -1, 0) - Vec3f::zero).Normalized();
    light_go.transform().forward(dir);

    auto main_light = light_go.AddComponent<DirectionalLight>(Color::kWhite, 1.0f);
    main_light->EnableShadow();

    auto cmd_queue = gfx->GetCommandQueue(CommandBufferType::kDirect);
    auto cmd_buffer = cmd_queue->GetCommandBuffer();

    //DrawHelmet(cmd_buffer);
    DrawSponza(cmd_buffer);

    cmd_queue->ExecuteCommandBuffer(cmd_buffer);
    cmd_queue->Flush();
}

}
}
