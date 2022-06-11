#include "renderable.h"
#include "exception/exception.h"
#include "Render/Graph/PassNode.h"
#include "Common/Util.h"
#include "imgui/imgui.h"
#include "Buffer.h"
#include "Render/Editor/Gizmos.h"

namespace glacier {
namespace render {

std::shared_ptr<ConstantBuffer> Renderable::tx_buf_;

int32_t Renderable::id_counter_ = 0;

Renderable::Renderable() :
    mask_(toUType(RenderableMask::kCastShadow) | toUType(RenderableMask::kReciveShadow))
{
}

Renderable::~Renderable() {
}

void Renderable::SetMaterial(const std::shared_ptr<Material>& mat) {
    material_ = mat;
}

void Renderable::UpdateTransform(GfxDriver* gfx) const {
    const auto& m = transform().LocalToWorldMatrix();
    const auto& mv = gfx->view() * m;
    const auto& mvp = gfx->projection() * mv;

    RenderableTransform tx_data = {
        m,
        mv,
        mvp,
        prev_model_,
        material_ ? material_->GetTexTilingOffset() : Vec4f{1.0f, 1.0f, 0.0f, 0.0f}
    };
    
    auto& tx_cbuf = GetTransformCBuffer(gfx);
    tx_cbuf->Update(&tx_data);

    prev_model_ = m;
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

std::shared_ptr<ConstantBuffer>& Renderable::GetTransformCBuffer(GfxDriver* gfx) {
    if (!tx_buf_) {
        tx_buf_ = gfx->CreateConstantBuffer<RenderableTransform>();
    }

    return tx_buf_;
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
        AABB(Vector3::zero, Vector3::zero);
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
