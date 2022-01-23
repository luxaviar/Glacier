#include "gfxdriver.h"
#include "render/camera.h"
#include "render/material.h"
#include "inputlayout.h"
#include "pipelinestate.h"
#include "Render/MaterialTemplate.h"

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

}
}
