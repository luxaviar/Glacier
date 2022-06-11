#include "MeshRenderer.h"
#include "Render/Graph/PassNode.h"
#include "Common/Util.h"
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags
#include "render/editor/gizmos.h"
#include "imgui/imgui.h"

namespace glacier {
namespace render {

MeshRenderer::MeshRenderer(const std::shared_ptr<Mesh>& mesh, const std::shared_ptr<Material>& material) {
    local_bounds_ = mesh->bounds();
    meshes_.push_back(mesh);

    SetMaterial(material);
}

void MeshRenderer::AddMesh(const std::shared_ptr<Mesh>& mesh) {
    assert(std::find(meshes_.begin(), meshes_.end(), mesh) == meshes_.end());

    meshes_.push_back(mesh);
    local_bounds_ = AABB::Union(local_bounds_, mesh->bounds());

    if (!IsHidden()) {
        RenderableManager::Instance()->UpdateBvhNode(this);
    }
}

void MeshRenderer::OnAwake() {
    if (!IsHidden()) {
        transform().OnUpdate([this](const Transform& tf) {
            RenderableManager::Instance()->UpdateBvhNode(this);
        });
    }
}

void MeshRenderer::OnEnable() {
    if (!IsHidden()) {
        RenderableManager::Instance()->UpdateBvhNode(this);
    }
}

void MeshRenderer::OnDisable() {
    RenderableManager::Instance()->RemoveBvhNode(this);
}


void MeshRenderer::Render(GfxDriver* gfx, Material* mat) const {
    UpdateTransform(gfx);

    MaterialGuard guard(gfx, mat ? mat : material_.get());
    for (auto& mesh : meshes_) {
        mesh->Draw(gfx);
    }
}

void MeshRenderer::Draw(GfxDriver* gfx) const {
    for (auto& mesh : meshes_) {
        mesh->Draw(gfx);
    }
}

void MeshRenderer::OnDrawSelectedGizmos() {
    auto gizmo = Gizmos::Instance();
    auto& box = world_bounds();
    auto center = box.Center();
    gizmo->SetColor(Color{ 0.8f, 0.8f, 0.8f, 1.0f });
    gizmo->DrawCube(center, box.Extent());
}

void MeshRenderer::DrawInspector() {
    if (ImGui::CollapsingHeader("MeshRenderer", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawInspectorBasic();

        if (meshes_.empty()) return;
        ImGui::Text("Meshes");
        for (auto mesh : meshes_) {
            ImGui::Bullet();
            ImGui::Selectable(mesh->name().c_str());
        }
    }
}

}
}
