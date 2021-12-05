#include "renderable.h"
#include "exception/exception.h"
#include "Render/Graph/PassNode.h"
#include "Common/Util.h"
#include "imgui/imgui.h"
#include "ConstantBuffer.h"

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

void Renderable::SetMaterial(Material* mat) {
    material_ = mat;
}

void Renderable::Bind(GfxDriver* gfx, Transform* tx) const {
    const auto& m = tx ? tx->LocalToWorldMatrix() : transform().LocalToWorldMatrix();
    const auto& mv = gfx->view() * m;
    const auto& mvp = gfx->projection() * mv;
    RenderableTransform tx_data = {
        m,
        mv,
        mvp,
        material_ ? material_->GetTexTilingOffset() : Vec4f{1.0f, 1.0f, 0.0f, 0.0f}
    };
    
    auto& tx_cbuf = GetTransformCBuffer(gfx);
    tx_cbuf.Update(&tx_data);
    tx_cbuf.Bind(ShaderType::kVertex, 0);
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
    //Vec3f pos, scale;
    //Quaternion rot;
    //mat.Decompose(pos, rot, scale);

    //auto extents = local_bounds_.Extents();
    //extents = AABB::RotateExtents(extents, rot.ToMatrix());
    //auto center = local_bounds_.Center() + pos;
    //world_bounds_.SetCenterAndExtent(center, extents);
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

ConstantBuffer& Renderable::GetTransformCBuffer(GfxDriver* gfx) const {
    if (!tx_buf_) {
        tx_buf_ = gfx->CreateConstantBuffer<RenderableTransform>();// std::make_unique<ConstantBuffer<RenderableTransform>>(gfx);
    }

    return *tx_buf_;
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

}
}
