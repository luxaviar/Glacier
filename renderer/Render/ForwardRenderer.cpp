#include "ForwardRenderer.h"
#include <memory>
#include <assert.h>
#include <algorithm>
#include "fmt/format.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui.h"
#include "Math/Util.h"
#include "Render/Graph/PassNode.h"
#include "Render/Graph/ResourceEntry.h"
#include "Render/Mesh/model.h"
#include "Camera.h"
#include "Math/Vec3.h"
#include "Math/Mat4.h"
#include "CascadedShadowManager.h"
#include "Core/GameObject.h"
#include "Render/LightManager.h"
#include "Render/Base/Texture.h"
#include "Render/Base/SamplerState.h"
#include "Inspect/Profiler.h"
#include "Render/Base/RenderTarget.h"
#include "Common/BitUtil.h"
#include "Render/Base/SwapChain.h"
#include "Render/Base/RenderTexturePool.h"
#include "Render/Base/Program.h"
#include "Render/Base/CommandBuffer.h"

namespace glacier {
namespace render {

ForwardRenderer::ForwardRenderer(GfxDriver* gfx, MSAAType msaa) :
    Renderer(gfx, msaa == MSAAType::kNone ? AntiAliasingType::kNone :AntiAliasingType::kMSAA),
    msaa_(msaa),
    option_msaa_(msaa)
{
    if (msaa_ != MSAAType::kNone) {
        gfx_->CheckMSAA(1 << toUType(msaa_), sample_count_, quality_level_);
        assert(sample_count_ <= 8);
    }
}

void ForwardRenderer::Setup() {
    pbr_program_ = gfx_->CreateProgram("PBR", TEXT("ForwardLighting"), TEXT("ForwardLighting"));
    pbr_program_->SetInputLayout(Mesh::kDefaultLayout);

    SamplerState ss;
    ss.warpU = ss.warpV = WarpMode::kRepeat;
    pbr_program_->SetProperty("linear_sampler", ss);
    pbr_program_->AddPass("ForwardLighting");

    Renderer::Setup();

    InitMSAA();
    InitRenderGraph(gfx_);

    auto solid_mat = MaterialManager::Instance()->Get("solid");
    solid_mat->AddPass("solid");
}

std::shared_ptr<Material> ForwardRenderer::CreateLightingMaterial(const char* name) {
    return std::make_shared<Material>(name, pbr_program_);
}

bool ForwardRenderer::OnResize(uint32_t width, uint32_t height) {
    if (!Renderer::OnResize(width, height)) {
        return false;
    }

    msaa_render_target_->Resize(width, height);

    return true;
}

void ForwardRenderer::PreRender(CommandBuffer* cmd_buffer) {
    if (msaa_ != option_msaa_) {
        msaa_ = option_msaa_;
        OnChangeMSAA();
    }

    if (msaa_ != MSAAType::kNone) {
        msaa_render_target_->Clear(cmd_buffer);
    }

    Renderer::PreRender(cmd_buffer);
}

void ForwardRenderer::InitRenderTarget() {
    Renderer::InitRenderTarget();

    auto width = gfx_->GetSwapChain()->GetWidth();
    auto height = gfx_->GetSwapChain()->GetHeight();
    msaa_render_target_ = gfx_->CreateRenderTarget(width, height);

    if (msaa_ == MSAAType::kNone) {
        msaa_render_target_ = hdr_render_target_;
        return;
    }

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
    auto depthstencil_texture = gfx_->CreateTexture(depth_tex_desc);
    depthstencil_texture->SetName("msaa depth texture");

    msaa_render_target_->AttachDepthStencil(depthstencil_texture);
}

void ForwardRenderer::InitMSAA() {
    RasterStateDesc rs;
    rs.depthFunc = CompareFunc::kAlways;

    auto vert_shader = gfx_->CreateShader(ShaderType::kVertex, TEXT("MSAAResolve"));

    for (int i = (int)MSAAType::k2x; i < (int)MSAAType::kMax; ++i) {
        auto sample_count = 1 << toUType(msaa_);
        auto sample_count_str = fmt::format("{}", sample_count);

        auto pixel_shader = gfx_->CreateShader(ShaderType::kPixel, TEXT("MSAAResolve"), "main_ps",
            { {"MSAASamples_", sample_count_str.c_str()}, {nullptr, nullptr}});

        auto program = gfx_->CreateProgram("MSAA");
        program->SetShader(vert_shader);
        program->SetShader(pixel_shader);

        auto mat = std::make_shared<Material>("msaa resolve", program);
        mat->GetProgram()->SetRasterState(rs);

        mat->SetProperty("depth_buffer", msaa_render_target_->GetDepthStencil());
        mat->SetProperty("color_buffer", msaa_render_target_->GetColorAttachment(AttachmentPoint::kColor0));
        msaa_resolve_mat_[i] = mat;
    }
}

void ForwardRenderer::OnChangeMSAA() {
    if (msaa_ != MSAAType::kNone) {
        anti_aliasing_ = AntiAliasingType::kMSAA;
        gfx_->CheckMSAA(1 << toUType(msaa_), sample_count_, quality_level_);
        assert(sample_count_ <= 8);

        auto width = present_render_target_->width();
        auto height = present_render_target_->height();
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
        auto depthstencil_texture = gfx_->CreateTexture(depth_tex_desc);
        depthstencil_texture->SetName("msaa depth texture");

        msaa_render_target_->AttachDepthStencil(depthstencil_texture);
    }
    else {
        sample_count_ = 1;
        quality_level_ = 0;

        msaa_render_target_->AttachColor(AttachmentPoint::kColor0, hdr_render_target_->GetColorAttachment(AttachmentPoint::kColor0));
        msaa_render_target_->AttachDepthStencil(hdr_render_target_->GetDepthStencil());
    }

    msaa_render_target_->signal().Emit();
}

void ForwardRenderer::ResolveMSAA(CommandBuffer* cmd_buffer) {
    PerfSample("Resolve MSAA");
    if (msaa_ == MSAAType::kNone) return;

    auto& mat = msaa_resolve_mat_[toUType(msaa_)];
    PostProcess(cmd_buffer, hdr_render_target_, mat.get());
}

void ForwardRenderer::AddLightingPass() {
    render_graph_.AddPass("ForwardLighting",
        [&](PassNode& pass) {
        },
        [this](CommandBuffer* cmd_buffer, const PassNode& pass) {
            PerfSample("Lighting Pass");

            LightManager::Instance()->Update(cmd_buffer);
            csm_manager_->Update();

            pass.Render(cmd_buffer, visibles_);
        });
}

void ForwardRenderer::InitRenderGraph(GfxDriver* gfx) {
    AddShadowPass();
    AddLightingPass();

    LightManager::Instance()->AddSkyboxPass(this);
    editor_.RegisterHighLightPass(gfx, this);

    render_graph_.Compile();
}

void ForwardRenderer::DrawOptionWindow() {
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
