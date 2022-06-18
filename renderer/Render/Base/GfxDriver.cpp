#include "gfxdriver.h"
#include "render/camera.h"
#include "render/material.h"
#include "inputlayout.h"
#include "pipelinestate.h"
#include "Program.h"
#include "../Material.h"

namespace glacier {
namespace render {

GfxDriver* GfxDriver::driver_ = nullptr;

void GfxDriver::BindCamera(const Camera* cam) {
    camera_position_ = cam->position();
#ifdef GLACIER_REVERSE_Z
    projection_ = cam->projection_reversez();
#else
    projection_ = cam->projection();
#endif
    view_ = cam->view();
}

void GfxDriver::BindCamera(const Vector3& pos, const Matrix4x4& view, const Matrix4x4& projection) {
    camera_position_ = pos;
    projection_ = projection;
    view_ = view;
}

void GfxDriver::BindMaterial(Material* mat) {
    if (!mat) return;

    auto program = mat->GetProgram().get();
    if (material_ == mat) {
        program->RefreshTranstientBuffer(this);
        return;
    }

    if (program != program_) {
        if (program_) {
            program_->UnBindPSO(this);
        }

        program->BindPSO(this);
        program_ = program;
    }

    if (material_) {
        material_->UnBind(this);
    }

    material_ = mat;
    material_->Bind(this);

    return;
}

void GfxDriver::UnBindMaterial() {
    if (material_) {
        material_->UnBind(this);
    }

    if (program_) {
        program_->UnBindPSO(this);
    }

    material_ = nullptr;
    program_ = nullptr;
}


}
}
