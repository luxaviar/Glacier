#include "FXAA.h"
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

void FXAA::Setup(DeferredRenderer* renderer)
{
    auto& ldr_render_target = renderer->GetLdrRenderTarget();
    fxaa_param_ = renderer->driver()->CreateConstantParameter<FXAAParam, UsageType::kDefault>(0.0833f, 0.166f, 0.75f, 0.0f);
    fxaa_mat_ = std::make_shared<PostProcessMaterial>("FXAA", TEXT("FXAA"));
    fxaa_mat_->SetProperty("fxaa_param", fxaa_param_);
    fxaa_mat_->SetProperty("_PostSourceTexture", ldr_render_target->GetColorAttachment(AttachmentPoint::kColor0));
}

void FXAA::Execute(Renderer* renderer, CommandBuffer* cmd_buffer) {
    auto& present_render_target = renderer->GetPresentRenderTarget();
    Renderer::PostProcess(cmd_buffer, present_render_target, fxaa_mat_.get());
}

void FXAA::DrawOptionWindow() {
    auto& param = fxaa_param_.param();
    bool dirty = false;

    ImGui::Text("Contrast Threshold");
    ImGui::SameLine(140);
    if (ImGui::SliderFloat("##Contrast Threshold", &param.contrast_threshold, 0.0312f, 0.0833f)) {
        dirty = true;
    }

    ImGui::Text("Relative Threshold");
    ImGui::SameLine(140);
    if (ImGui::SliderFloat("##Relative Threshold", &param.relative_threshold, 0.063f, 0.333f)) {
        dirty = true;
    }

    ImGui::Text("Subpixel Blending");
    ImGui::SameLine(140);
    if (ImGui::SliderFloat("##Subpixel Blending", &param.subpixel_blending, 0.0f, 1.0f)) {
        dirty = true;
    }

    if (dirty) {
        fxaa_param_.Update();
    }
}

}
}
