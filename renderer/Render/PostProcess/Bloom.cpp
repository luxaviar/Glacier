#include "Bloom.h"
#include <assert.h>
#include <algorithm>
#include "Render/Renderer.h"
#include <imgui.h>
#include "Math/Util.h"
#include "Math/Vec3.h"
#include "Render/Base/Texture.h"
#include "Render/Base/SamplerState.h"
#include "Render/Base/Buffer.h"
#include "Render/Base/Program.h"
#include "Render/Base/RenderTexturePool.h"
#include "Common/Log.h"
#include "Render/Base/CommandBuffer.h"
#include "Render/Base/GfxDriver.h"

namespace glacier {
namespace render {

namespace {

RasterStateDesc AdditiveBlendState() {
    RasterStateDesc rs;
    rs.depthWrite = false;
    rs.depthEnable = false;
    rs.depthFunc = CompareFunc::kAlways;

    rs.blendEquationRGB = BlendEquation::kAdd;
    rs.blendFunctionSrcRGB = BlendFunction::kOne;
    rs.blendFunctionDstRGB = BlendFunction::kOne;

    //bloom is a color only effect, keep the alpha of the destination intact
    rs.blendEquationAlpha = BlendEquation::kAdd;
    rs.blendFunctionSrcAlpha = BlendFunction::kZero;
    rs.blendFunctionDstAlpha = BlendFunction::kOne;

    return rs;
}

}

void Bloom::Setup(Renderer* renderer)
{
    gfx_ = renderer->driver();

    auto& hdr_render_target = renderer->GetHdrRenderTarget();
    auto width = hdr_render_target->width();
    auto height = hdr_render_target->height();
    hdr_texture_ = hdr_render_target->GetColorAttachment(AttachmentPoint::kColor0);

    //every pass of the pyramid writes its own texel sizes into the buffer, so it
    //has to be a transient one (a default buffer would be overwritten by the
    //next Update before the command list is executed)
    bloom_param_ = gfx_->CreateConstantParameter<BloomParam, UsageType::kDynamic>();

    prefilter_mat_ = std::make_shared<PostProcessMaterial>("Bloom Prefilter", TEXT("BloomPrefilter"));
    prefilter_mat_->SetProperty("bloom_params", bloom_param_);
    prefilter_mat_->SetProperty("_PostSourceTexture", hdr_texture_);

    composite_mat_ = std::make_shared<PostProcessMaterial>("Bloom Composite", TEXT("BloomComposite"));
    composite_mat_->SetProperty("bloom_params", bloom_param_);
    composite_mat_->GetProgram()->SetRasterState(AdditiveBlendState());

    ReallocMipChain(gfx_, width, height);

    UpdateParam(width, height, mips_[0]->width(), mips_[0]->height());
}

void Bloom::ReallocMipChain(GfxDriver* gfx, uint32_t width, uint32_t height)
{
    downsample_mats_.clear();
    upsample_mats_.clear();
    mips_.clear();
    mip_render_targets_.clear();

    uint32_t mip_width = std::max(1u, width / 2);
    uint32_t mip_height = std::max(1u, height / 2);

    for (int i = 0; i < kMaxMipCount; ++i)
    {
        auto texture = RenderTexturePool::Get(mip_width, mip_height, TextureFormat::kR16G16B16A16_FLOAT);
        texture->SetName("bloom mip texture");

        auto render_target = gfx->CreateRenderTarget(mip_width, mip_height);
        render_target->AttachColor(AttachmentPoint::kColor0, texture);

        mips_.push_back(texture);
        mip_render_targets_.push_back(render_target);

        if (mip_width <= kMinMipSize || mip_height <= kMinMipSize)
            break;

        mip_width = std::max(1u, mip_width / 2);
        mip_height = std::max(1u, mip_height / 2);
    }

    auto mip_count = (int)mips_.size();

    //downsample mips_[i - 1] into mips_[i]
    for (int i = 1; i < mip_count; ++i)
    {
        auto mat = std::make_shared<PostProcessMaterial>("Bloom Downsample", TEXT("BloomDownsample"));
        mat->SetProperty("bloom_params", bloom_param_);
        mat->SetProperty("_PostSourceTexture", mips_[i - 1]);
        downsample_mats_.push_back(mat);
    }

    //bloom mips_[i + 1] on top of mips_[i]
    for (int i = 0; i < mip_count - 1; ++i)
    {
        auto mat = std::make_shared<PostProcessMaterial>("Bloom Upsample", TEXT("BloomUpsample"));
        mat->SetProperty("bloom_params", bloom_param_);
        mat->SetProperty("_PostSourceTexture", mips_[i + 1]);
        mat->GetProgram()->SetRasterState(AdditiveBlendState());
        upsample_mats_.push_back(mat);
    }

    composite_mat_->SetProperty("BloomTexture", mips_[0]);
}

void Bloom::OnResize(uint32_t width, uint32_t height)
{
    assert(gfx_);

    ReallocMipChain(gfx_, width, height);
    UpdateParam(width, height, mips_[0]->width(), mips_[0]->height());
}

void Bloom::UpdateParam(uint32_t src_width, uint32_t src_height, uint32_t dst_width, uint32_t dst_height)
{
    auto& param = bloom_param_.param();
    float knee = std::max(soft_knee_, 0.0001f);

    param._BloomThreshold = Vec4f(threshold_, threshold_ - knee, knee * 2.0f, 0.25f / knee);
    param._BloomParams = Vec4f(scatter_, intensity_,
        1.0f / (float)mips_[0]->width(), 1.0f / (float)mips_[0]->height());
    param._BloomTexelSize = Vec4f(1.0f / (float)src_width, 1.0f / (float)src_height,
        1.0f / (float)dst_width, 1.0f / (float)dst_height);
    param._BloomClamp = Vec4f(max_brightness_, 0.0f, 0.0f, 0.0f);

    bloom_param_.Update();
}

void Bloom::Execute(Renderer* renderer, CommandBuffer* cmd_buffer)
{
    if (!enabled_ || mips_.empty())
        return;

    auto& hdr_render_target = renderer->GetHdrRenderTarget();
    auto hdr_texture = hdr_render_target->GetColorAttachment(AttachmentPoint::kColor0);

    //bright pass: hdr frame -> the smallest resolution but largest bloom mip
    UpdateParam(hdr_texture->width(), hdr_texture->height(), mips_[0]->width(), mips_[0]->height());
    Renderer::PostProcess(cmd_buffer, mip_render_targets_[0], prefilter_mat_.get());

    for (size_t i = 1; i < mips_.size(); ++i)
    {
        UpdateParam(mips_[i - 1]->width(), mips_[i - 1]->height(), mips_[i]->width(), mips_[i]->height());
        Renderer::PostProcess(cmd_buffer, mip_render_targets_[i], downsample_mats_[i - 1].get());
    }

    //the mips are added back from the smallest to the largest one
    for (size_t i = mips_.size() - 1; i > 0; --i)
    {
        auto dst = i - 1;
        UpdateParam(mips_[i]->width(), mips_[i]->height(), mips_[dst]->width(), mips_[dst]->height());
        Renderer::PostProcess(cmd_buffer, mip_render_targets_[dst], upsample_mats_[dst].get());
    }

    //blend the bloom additively into the hdr frame, tonemapping runs afterwards
    UpdateParam(mips_[0]->width(), mips_[0]->height(), hdr_texture->width(), hdr_texture->height());
    Renderer::PostProcess(cmd_buffer, hdr_render_target, composite_mat_.get());
}

void Bloom::DrawOptionWindow()
{
    if (ImGui::CollapsingHeader("Bloom", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Enable", &enabled_);

        ImGui::Text("Intensity");
        ImGui::SameLine(120);
        ImGui::SliderFloat("##Bloom Intensity", &intensity_, 0.0f, 5.0f);

        ImGui::Text("Threshold");
        ImGui::SameLine(120);
        ImGui::SliderFloat("##Bloom Threshold", &threshold_, 0.0f, 10.0f);

        ImGui::Text("Soft Knee");
        ImGui::SameLine(120);
        ImGui::SliderFloat("##Bloom Soft Knee", &soft_knee_, 0.0f, 1.0f);

        ImGui::Text("Scatter");
        ImGui::SameLine(120);
        ImGui::SliderFloat("##Bloom Scatter", &scatter_, 0.0f, 1.0f);

        ImGui::Text("Max Brightness");
        ImGui::SameLine(120);
        ImGui::SliderFloat("##Bloom Max Brightness", &max_brightness_, 1.0f, 200.0f);
    }
}

}
}
