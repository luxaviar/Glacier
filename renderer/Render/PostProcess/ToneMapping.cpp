#include "ToneMapping.h"
#include <assert.h>
#include <algorithm>
#include "Render/DeferredRenderer.h"
#include "imgui/imgui.h"
#include "Math/Util.h"
#include "Math/Vec3.h"
#include "Math/Mat4.h"
#include "Core/GameObject.h"
#include "Core/ObjectManager.h"
#include "Render/Base/SamplerState.h"
#include "Render/Base/Buffer.h"
#include "Render/Base/SwapChain.h"
#include "Render/Base/Program.h"
#include "Render/Base/RenderTexturePool.h"
#include "Common/Log.h"
#include "Render/Base/CommandBuffer.h"
#include "Render/Base/CommandQueue.h"
#include "Render/Base/GfxDriver.h"

namespace glacier {
namespace render {

void ToneMapping::Setup(Renderer* renderer, const std::shared_ptr<Buffer>& exposure_buffer)
{
    auto& hdr_render_target_ = renderer->GetHdrRenderTarget();
    auto& ldr_render_target_ = renderer->GetLdrRenderTarget();

    tonemapping_mat_ = std::make_shared<Material>("ToneMapCS", TEXT("ToneMapCS"));

    tonemap_buf_ = renderer->driver()->CreateConstantParameter<ToneMapParam, UsageType::kDefault>();
    tonemap_buf_ = { 1.0f / hdr_render_target_->width(), 1.0f / hdr_render_target_->height(), 0.1f, 200.0f / 1000.0f, 1000.0f };

    tonemapping_mat_->SetProperty("tonemap_params", tonemap_buf_);
    tonemapping_mat_->SetProperty("Exposure", exposure_buffer);
    tonemapping_mat_->SetProperty("SrcColor", hdr_render_target_->GetColorAttachment(AttachmentPoint::kColor0));
    tonemapping_mat_->SetProperty("DstColor", ldr_render_target_->GetColorAttachment(AttachmentPoint::kColor0));
}

void ToneMapping::OnResize(uint32_t width, uint32_t height) {
    tonemap_buf_ = { 1.0f / width, 1.0f / height, 0.1f, 200.0f / 1000.0f, 1000.0f };
}

void ToneMapping::Execute(Renderer* renderer, CommandBuffer* cmd_buffer) {
    auto& hdr_render_target_ = renderer->GetHdrRenderTarget();
    auto dst_width = hdr_render_target_->width();
    auto dst_height = hdr_render_target_->height();

    cmd_buffer->BindMaterial(tonemapping_mat_.get());
    cmd_buffer->Dispatch(math::DivideByMultiple(dst_width, 8), math::DivideByMultiple(dst_height, 8), 1);
}

void ToneMapping::DrawOptionWindow() {
}

}
}
