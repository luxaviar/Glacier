#include "CommandBuffer.h"
#include "Render/Camera.h"
#include "Render/Material.h"
#include "Render/Base/Program.h"
#include "Lux/Lux.h"

namespace glacier {
namespace render {

LUX_IMPL(CommandBuffer, CommandBuffer)
//LUX_CTOR(CommandBuffer, GfxDriver*, CommandBufferType)
LUX_IMPL_END

CommandBuffer::CommandBuffer(GfxDriver* driver, CommandBufferType type) :
    driver_(driver),
    type_(type)
{

}

void CommandBuffer::BindMaterial(Material* mat) {
    if (!mat) return;

    auto program = mat->GetProgram().get();
    if (material_ == mat) {
        program->RefreshTranstientBuffer(this);
        return;
    }

    if (program != program_) {
        program->BindPSO(this);
        program_ = program;
    }

    material_ = mat;
    material_->Bind(this);

    return;
}

void CommandBuffer::UnBindMaterial() {
    material_ = nullptr;
    program_ = nullptr;
}

void CommandBuffer::BindCamera(const Camera* cam) {
    camera_position_ = cam->position();
#ifdef GLACIER_REVERSE_Z
    projection_ = cam->projection_reversez();
#else
    projection_ = cam->projection();
#endif
    view_ = cam->view();
}

void CommandBuffer::BindCamera(const Vector3& pos, const Matrix4x4& view, const Matrix4x4& projection) {
    camera_position_ = pos;
    projection_ = projection;
    view_ = view;
}


}
}
