#include "MSAA.h"
#include <assert.h>
#include <algorithm>
#include "Render/Renderer.h"
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

MSAA::MSAA(MSAAType msaa) : 
    msaa_(msaa),
    option_msaa_(msaa)
{
}

void MSAA::Setup(Renderer* renderer)
{
    auto gfx = renderer->driver();
    if (msaa_ != MSAAType::kNone) {
        gfx->CheckMSAA(1 << toUType(msaa_), sample_count_, quality_level_);
        assert(sample_count_ <= 8);
    }

    auto& hdr_render_target_ = renderer->GetHdrRenderTarget();
    auto width = hdr_render_target_->width();
    auto height = hdr_render_target_->height();

    if (msaa_ == MSAAType::kNone) {
        msaa_render_target_ = hdr_render_target_;
    } else {
        msaa_render_target_ = gfx->CreateRenderTarget(width, height);
        auto msaa_color = RenderTexturePool::Get(width, height, TextureFormat::kR16G16B16A16_FLOAT,
            CreateFlags::kRenderTarget, sample_count_, quality_level_);
        msaa_color->SetName("msaa color buffer");

        msaa_render_target_->AttachColor(AttachmentPoint::kColor0, msaa_color);

        auto depth_tex_desc = Texture::Description()
            .SetDimension(width, height)
            .SetFormat(TextureFormat::kR24G8_TYPELESS)
            .SetCreateFlag(CreateFlags::kDepthStencil)
            .SetCreateFlag(CreateFlags::kShaderResource)
            .SetSampleDesc(sample_count_, quality_level_);
        auto depthstencil_texture = gfx->CreateTexture(depth_tex_desc);
        depthstencil_texture->SetName("msaa depth texture");

        msaa_render_target_->AttachDepthStencil(depthstencil_texture);
    }

    RasterStateDesc rs;
    rs.depthFunc = CompareFunc::kAlways;
    auto vert_shader = gfx->CreateShader(ShaderType::kVertex, TEXT("MSAAResolve"));
    for (int i = (int)MSAAType::k2x; i < (int)MSAAType::kMax; ++i) {
        auto sample_count = 1 << toUType(msaa_);
        auto sample_count_str = fmt::format("{}", sample_count);

        auto pixel_shader = gfx->CreateShader(ShaderType::kPixel, TEXT("MSAAResolve"), "main_ps",
            { {"MSAASamples_", sample_count_str.c_str()}, {nullptr, nullptr}});

        auto program = gfx->CreateProgram("MSAA");
        program->SetShader(vert_shader);
        program->SetShader(pixel_shader);

        auto mat = std::make_shared<Material>("msaa resolve", program);
        mat->GetProgram()->SetRasterState(rs);

        mat->SetProperty("depth_buffer", msaa_render_target_->GetDepthStencil());
        mat->SetProperty("color_buffer", msaa_render_target_->GetColorAttachment(AttachmentPoint::kColor0));
        msaa_resolve_mat_[i] = mat;
    }
}

void MSAA::PreRender(Renderer* renderer, CommandBuffer* cmd_buffer) {
    if (msaa_ != option_msaa_) {
        msaa_ = option_msaa_;
        OnChangeMSAA(renderer);
    }

    if (msaa_ != MSAAType::kNone) {
        msaa_render_target_->Clear(cmd_buffer);
    }
}

void MSAA::OnChangeMSAA(Renderer* renderer) {
    if (msaa_ != MSAAType::kNone) {
        renderer->SetAntiAliasingType(AntiAliasingType::kMSAA);
        auto gfx = renderer->driver();

        gfx->CheckMSAA(1 << toUType(msaa_), sample_count_, quality_level_);
        assert(sample_count_ <= 8);

        auto& present_render_target = renderer->GetPresentRenderTarget();
        auto width = present_render_target->width();
        auto height = present_render_target->height();
        auto msaa_color = RenderTexturePool::Get(width, height, TextureFormat::kR16G16B16A16_FLOAT,
            CreateFlags::kRenderTarget, sample_count_, quality_level_);
        msaa_color->SetName("msaa color buffer");

        msaa_render_target_->AttachColor(AttachmentPoint::kColor0, msaa_color);

        auto depth_tex_desc = Texture::Description()
            .SetDimension(width, height)
            .SetFormat(TextureFormat::kR24G8_TYPELESS)
            .SetCreateFlag(CreateFlags::kDepthStencil)
            .SetCreateFlag(CreateFlags::kShaderResource)
            .SetSampleDesc(sample_count_, quality_level_);
        auto depthstencil_texture = gfx->CreateTexture(depth_tex_desc);
        depthstencil_texture->SetName("msaa depth texture");

        msaa_render_target_->AttachDepthStencil(depthstencil_texture);
    }
    else {
        sample_count_ = 1;
        quality_level_ = 0;

        auto& hdr_render_target = renderer->GetHdrRenderTarget();
        msaa_render_target_->AttachColor(AttachmentPoint::kColor0, hdr_render_target->GetColorAttachment(AttachmentPoint::kColor0));
        msaa_render_target_->AttachDepthStencil(hdr_render_target->GetDepthStencil());
    }

    msaa_render_target_->signal().Emit();
}

void MSAA::OnResize(uint32_t width, uint32_t height) {
    msaa_render_target_->Resize(width, height);
}

void MSAA::Execute(Renderer* renderer, CommandBuffer* cmd_buffer) {
    if (msaa_ == MSAAType::kNone) return;

    auto& mat = msaa_resolve_mat_[toUType(msaa_)];
    auto& hdr_render_target = renderer->GetHdrRenderTarget();
    Renderer::PostProcess(cmd_buffer, hdr_render_target, mat.get());
}

void MSAA::DrawOptionWindow() {
    if (ImGui::CollapsingHeader("Anti-Aliasing", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* combo_label = kMsaaDesc[(int)option_msaa_];

        ImGui::Text("MSAA");
        ImGui::SameLine(80);
        if (ImGui::BeginCombo("##MSAA", combo_label, ImGuiComboFlags_PopupAlignLeft)) {
            for (int n = 0; n < kMsaaDesc.size(); n++) {
                const bool is_selected = ((int)option_msaa_ == n);

                if (ImGui::Selectable(kMsaaDesc[n], is_selected))
                    option_msaa_ = (MSAAType)n;

                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }
}

}
}
