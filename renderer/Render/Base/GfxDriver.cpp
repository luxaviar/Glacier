#include "gfxdriver.h"
#include "render/camera.h"
#include "render/material.h"
#include "inputlayout.h"
#include "pipelinestate.h"

namespace glacier {
namespace render {

GfxDriver* GfxDriver::driver_ = nullptr;

void GfxDriver::BindCamera(const Camera* cam) {
    projection_ = cam->projection();
    view_ = cam->view();
}

void GfxDriver::BindCamera(const Matrix4x4& view, const Matrix4x4& projection) {
    projection_ = projection;
    view_ = view;
}

void GfxDriver::UpdateInputLayout(const std::shared_ptr<InputLayout>& layout) {
    if (input_layout_ == layout.get()) return;

    input_layout_ = layout.get();
    input_layout_->Bind();
}

void GfxDriver::UpdatePipelineState(const RasterState& rs) {
    if (raster_state_ == rs) {
        return;
    }

    auto pso = this->CreatePipelineState(rs);
    pso->Bind();
    raster_state_ = rs;
}

bool GfxDriver::PushMaterial(Material* mat) {
    if (!mat) return false;

    if (!materials_.empty()) {
        auto cur_mat = materials_.top();
        if (cur_mat == mat) {
            cur_mat->ReBind(this);
            return false;
        }
    }

    materials_.push(mat);
    mat->Bind(this);

    return true;
}

void GfxDriver::PopMaterial(Material* mat) {
    if (materials_.empty()) return;

    auto cur_mat = materials_.top();
    if (cur_mat == mat) {
        cur_mat->UnBind();
        materials_.pop();
    }
}

void GfxDriver::PopMaterial(int n) {
    assert(materials_.size() >= n);
    for (int i = 0; i < n; ++i) {
        auto mat = materials_.top();
        PopMaterial(mat);
    }
}

}
}
