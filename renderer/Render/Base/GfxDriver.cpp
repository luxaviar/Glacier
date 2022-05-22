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

}
}
