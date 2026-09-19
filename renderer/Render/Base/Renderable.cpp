#include "renderable.h"
#include "exception/exception.h"
#include "Render/Graph/PassNode.h"
#include "Render/Skinning/InstanceBuffer.h"
#include "Common/Util.h"
#include <imgui.h>
#include "Buffer.h"
#include "Render/Editor/Gizmos.h"

namespace glacier {
namespace render {

std::shared_ptr<Buffer> Renderable::per_object_data_;

int32_t Renderable::id_counter_ = 0;

void Renderable::Setup() {
    per_object_data_ = GfxDriver::Get()->CreateConstantBuffer<PerObjectData>();
    BoneMatrixPool::Instance()->Setup();
}

Renderable::Renderable() :
    mask_(toUType(RenderableMask::kCastShadow) | toUType(RenderableMask::kReciveShadow))
{
}

Renderable::~Renderable() {
}

void Renderable::SetMaterial(const std::shared_ptr<Material>& mat) {
    material_ = mat;
}

void Renderable::UpdateRenderData() const {
    CaptureFrameModel();
}

void Renderable::CaptureFrameModel() const {
    const auto& m = transform().LocalToWorldMatrix();

    //an object that was never captured has no previous pose, so it reports no
    //motion instead of the identity matrix
    prev_model_ = frame_model_valid_ ? frame_model_ : m;
    frame_model_ = m;
    frame_model_valid_ = true;
}

void Renderable::UpdatePerObjectData(CommandBuffer* cmd_buffer) const {
    UpdateBatchData(cmd_buffer, 0);
}

void Renderable::UpdateBatchData(CommandBuffer* cmd_buffer, uint32_t instance_offset) const {
    //a renderable created after the per frame pass has no snapshot yet
    if (!frame_model_valid_) {
        CaptureFrameModel();
    }

    const auto& m = transform().LocalToWorldMatrix();
    const auto& mv = cmd_buffer->view() * m;
    const auto& mvp = cmd_buffer->projection() * mv;

    PerObjectData data = {
        m,
        mv,
        mvp,
        prev_model_,
        material_ ? material_->GetTexTilingOffset() : Vec4f{1.0f, 1.0f, 0.0f, 0.0f},
        bone_offset(),
        prev_bone_offset(),
        instance_offset,
        0
    };

    per_object_data_->Update(&data);
}

const AABB& Renderable::world_bounds() const {
    //FIXME: deal with scale
    if (bounds_version_ != transform().version()) {
        UpdateWorldBounds();
    }

    return world_bounds_;
}

void Renderable::UpdateWorldBounds() const {
    auto& mat = transform().LocalToWorldMatrix();
    world_bounds_ = AABB::Transform(local_bounds_, mat);
    bounds_version_ = transform().version();
}

void Renderable::SetCastShadow(bool on) {
    if (on) {
        mask_ |= toUType(RenderableMask::kCastShadow);
    } else {
        mask_ &= ~toUType(RenderableMask::kCastShadow);
    }
}

void Renderable::SetReciveShadow(bool on) {
    if (on) {
        mask_ |= toUType(RenderableMask::kReciveShadow);
    } else {
        mask_ &= ~toUType(RenderableMask::kReciveShadow);
    }
}

void Renderable::SetPickable(bool on) {
    if (on) {
        mask_ &= ~toUType(RenderableMask::kUnPickable);
    } else {
        mask_ |= toUType(RenderableMask::kUnPickable);
    }
}

std::shared_ptr<Buffer>& Renderable::GetPerObjectData() {
    return per_object_data_;
}

void Renderable::DrawInspectorBasic() {
    ImGui::Text("Material");
    ImGui::SameLine(80);
    if (ImGui::Selectable(material_ ? material_->name().c_str() : "null")) {
        if (material_) {
            ImGui::OpenPopup("renderable/material");
        }
    }

    if (ImGui::BeginPopup("renderable/material")) {
        if (material_) material_->DrawInspector();
        ImGui::EndPopup();
    }
}

AABB RenderableManager::SceneBounds() {
    auto root = tree_.root();
    if (root) {
        return root->bounds;
    }
    else {
        return AABB(Vector3::zero, Vector3::zero);
    }
}

bool RenderableManager::Cull(const Camera& camera, std::vector<Renderable*>& result, const CullFilter& filter) {
    Frustum frustum(camera);
    return Cull(frustum, result, filter);
}

bool RenderableManager::Cull(const Frustum& frustum, std::vector<Renderable*>& result, const CullFilter& filter) {
    if (filter) {
        return tree_.QueryByFrustum(frustum, result,
            [&filter](const RenderableTree::NodeType* node){
                auto renderable = node->data;
                return filter(renderable);
            });
    }
    else {
        return tree_.QueryByFrustum(frustum, result);
    }

}

void RenderableManager::UpdateBvhNode(Renderable* o) {
    if (o->node_) {
        tree_.UpdateLeaf(o->node_, o->world_bounds(), Vector3::zero, false, true);
    }
    else {
        auto node = tree_.AddLeaf(o, o->world_bounds());
        o->node_ = node;
    }
}

void RenderableManager::UpdateRenderData() {
    //the bones of this frame go into the region that was last presented, so
    //switch before anything uploads or draws
    BoneMatrixPool::Instance()->BeginFrame();
    InstanceBuffer::Instance()->BeginFrame();

    for (auto it = objects_.begin(); it != objects_.end(); ++it) {
        it->data->UpdateRenderData();
    }
}

void RenderableManager::UpdateSkinning(CommandBuffer* cmd_buffer) {
    for (auto it = objects_.begin(); it != objects_.end(); ++it) {
        it->data->DispatchSkinning(cmd_buffer);
    }
}

void RenderableManager::RemoveBvhNode(Renderable* o) {
    if (o->node_) {
        tree_.RemoveLeaf(o->node_);
        o->node_ = nullptr;
    }
}

void RenderableManager::OnDrawGizmos() {
    auto root = tree_.root();
    DrawNode(root);
}

void RenderableManager::DrawNode(const RenderableTreeNode* node) {
    if (!node) return;

    auto& gizmo = *render::Gizmos::Instance();

    gizmo.SetColor(Color::kGreen);
    gizmo.DrawCube(node->bounds.Center(), node->bounds.Extent());

    DrawNode(node->left);
    DrawNode(node->right);
}

}
}
