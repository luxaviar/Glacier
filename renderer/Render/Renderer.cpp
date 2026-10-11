#include "Renderer.h"
#include <memory>
#include <assert.h>
#include <algorithm>
#include <imgui_impl_win32.h>
#include <imgui.h>
#include "Math/Util.h"
#include "Render/Graph/PassNode.h"
#include "Render/Mesh/MeshRenderer.h"
#include "Camera.h"
#include "Math/Vec3.h"
#include "Math/Mat4.h"
#include "Core/GameObject.h"
#include "Core/ObjectManager.h"
#include "CascadedShadowManager.h"
#include "LightManager.h"
#include "Core/Scene.h"
#include "physics/World.h"
#include "Input/Input.h"
#include "Common/BitUtil.h"
#include "Render/Image.h"
#include "Render/Base/Renderable.h"
#include "Render/Base/RenderTarget.h"
#include "Render/Base/SamplerState.h"
#include "Render/Base/Buffer.h"
#include "Render/Base/SwapChain.h"
#include "Render/Base/Program.h"
#include "Render/Base/RenderTexturePool.h"
#include "Inspect/Profiler.h"
#include "Common/Log.h"
#include "Render/Base/CommandBuffer.h"
#include "Render/Base/CommandQueue.h"
#include "Render/Base/GfxDriver.h"
#include "Lux/Lux.h"

namespace glacier {
namespace render {

LUX_IMPL(Renderer, Renderer)
LUX_FUNC(Renderer, driver)
LUX_IMPL_END

void Renderer::PostProcess(CommandBuffer* cmd_buffer, const std::shared_ptr<RenderTarget>& dst, Material* mat, bool color_only)
{
    RenderTargetGuard dst_guard(cmd_buffer, dst.get(), color_only);
    cmd_buffer->BindMaterial(mat);
    cmd_buffer->DrawInstanced(3, 1, 0, 0);
}

Renderer::Renderer(GfxDriver* gfx, AntiAliasingType aa) :
    gfx_(gfx),
    anti_aliasing_(aa),
    editor_(gfx),
    post_process_manager_(gfx)
{
    per_frame_param_ = gfx->CreateConstantParameter<PerFrameData, UsageType::kDynamic>();
    stats_ = std::make_unique<PerfStats>(gfx);
    csm_manager_ = std::make_shared<CascadedShadowManager>(gfx, 1024, std::vector<float>{ 10.6, 18.6, 19.2, 51.6 });
}

void Renderer::Setup() {
    Renderable::Setup();

    InitRenderTarget();
    InitMaterial();

    Gizmos::Instance()->Setup(gfx_);
    LightManager::Instance()->Setup(gfx_);
    exposure_.Setup(this);
    tonemapping_.Setup(this, exposure_.GetExposureBuffer());
    bloom_.Setup(this);

    auto cmd_buffer = gfx_->GetCommandBuffer(CommandBufferType::kCopy);

    InitFloorPbr(cmd_buffer);
    InitDefaultPbr(cmd_buffer);

    auto cmd_queue = gfx_->GetCommandQueue(CommandBufferType::kCopy);
    cmd_queue->ExecuteCommandBuffer(cmd_buffer);
    cmd_queue->Flush();
}

void Renderer::InitRenderTarget() {
    auto width = gfx_->GetSwapChain()->GetWidth();
    auto height = gfx_->GetSwapChain()->GetHeight();
    present_render_target_ = gfx_->GetSwapChain()->GetRenderTarget();
    auto backbuffer_depth_tex = present_render_target_->GetDepthStencil();

    auto hdr_colorframe = RenderTexturePool::Get(width, height, TextureFormat::kR16G16B16A16_FLOAT,
        CreateFlags::kRenderTarget);
    hdr_colorframe->SetName("hdr color frame");

    hdr_render_target_ = gfx_->CreateRenderTarget(width, height);
    hdr_render_target_->AttachColor(AttachmentPoint::kColor0, hdr_colorframe);
    hdr_render_target_->AttachDepthStencil(backbuffer_depth_tex);

    ldr_render_target_ = gfx_->CreateRenderTarget(width, height);
    auto ldr_colorframe = RenderTexturePool::Get(width, height, TextureFormat::kR8G8B8A8_UNORM,
        (CreateFlags)((uint32_t)CreateFlags::kUav | (uint32_t)CreateFlags::kShaderResource | (uint32_t)CreateFlags::kRenderTarget));

    ldr_colorframe->SetName("ldr color frame");
    ldr_render_target_->AttachColor(AttachmentPoint::kColor0, ldr_colorframe);
    ldr_render_target_->AttachDepthStencil(backbuffer_depth_tex);

    //the targets the graph's passes declare (T1.7): PreRender clears both
    //before the graph runs, so that is the state the frame starts in; hdr is
    //read (TAA, luminance, bloom) after the graph, so the graph hands it back
    //readable
    hdr_color_ = render_graph_.Import("hdr color",
        hdr_render_target_->GetColorAttachment(AttachmentPoint::kColor0).get(),
        ResourceAccessBit::kRenderTarget, ResourceAccessBit::kPixelShaderRead,
        //the frame is consumed after the graph (TAA, luminance, bloom), so a
        //cull must keep whatever produces it
        true);
    hdr_depth_ = render_graph_.Import("hdr depth", hdr_render_target_->GetDepthStencil().get(),
        ResourceAccessBit::kDepthWrite, ResourceAccessBit::kDepthWrite);

    render_graph_.SetRenderSize(width, height);

    per_frame_param_.param()._ScreenParam = { (float)width, (float)height, 1.0f / (float)width, 1.0f / (float)height };
}

bool Renderer::OnResize(uint32_t width, uint32_t height) {
    auto swapchain = gfx_->GetSwapChain();
    if (swapchain->GetWidth() == width && swapchain->GetHeight() == height) {
        return false;
    }

    swapchain->OnResize(width, height);

    hdr_render_target_->Resize(width, height);
    ldr_render_target_->Resize(width, height);

    editor_.OnResize(width, height);
    gtao_.OnResize(width, height);
    exposure_.OnResize(width, height);
    tonemapping_.OnResize(width, height);
    bloom_.OnResize(width, height);

    //transients follow the render size; the graph recompiles on the next frame
    render_graph_.SetRenderSize(width, height);

    per_frame_param_.param()._ScreenParam = { (float)width, (float)height, 1.0f / (float)width, 1.0f / (float)height };
    return true;
}

void Renderer::FilterVisibles() {
    PerfSample("Filter Visibles");
    auto main_camera = GetMainCamera();

    visibles_.clear();
    RenderableManager::Instance()->Cull(*main_camera, visibles_);

    //TODO: sort by depth?
    std::sort(visibles_.begin(), visibles_.end(), [](Renderable* a, Renderable* b) {
        auto template_a = a->GetMaterial()->GetProgram()->id();
        auto template_b = b->GetMaterial()->GetProgram()->id();
        if (template_a == template_b) {
            return a->GetMaterial()->id() < b->GetMaterial()->id();
        }

        return template_a < template_b;
    });
}

void Renderer::BindLightingTarget(CommandBuffer* cmd_buffer) {
    GetLightingRenderTarget()->Bind(cmd_buffer);
}

void Renderer::BindMainCamera(CommandBuffer* cmd_buffer) {
    auto main_camera = GetMainCamera();
    cmd_buffer->BindCamera(main_camera);
}

void Renderer::InitMaterial() {
    auto solid_mat = std::make_unique<Material>("solid", TEXT("Solid"), TEXT("Solid"));
    solid_mat->SetProperty("color", Color{ 1.0f,1.0f,1.0f, 1.0f });
    solid_mat->GetProgram()->SetInputLayout(InputLayoutDesc{ InputLayoutDesc::Position3D });

    MaterialManager::Instance()->Add(std::move(solid_mat));
}

Camera* Renderer::GetMainCamera() const {
    auto scene = SceneManager::Instance()->CurrentScene();
    if (!scene) {
        return nullptr;
    }

    return scene->GetMainCamera();
}

void Renderer::UpdatePerFrameData() {
    auto camera = GetMainCamera();
    auto view = camera->view();
#ifdef GLACIER_REVERSE_Z
    auto projection = camera->projection_reversez();
#else
    auto projection = camera->projection();
#endif

    auto& param = per_frame_param_.param();
    prev_view_projection_ = projection * view;
    param._UnjitteredViewProjection = prev_view_projection_;
    param._UnjitteredInverseViewProjection = prev_view_projection_.Inverted().value();
    param._UnjitteredInverseProjection = projection.Inverted().value();

    if (anti_aliasing_ == AntiAliasingType::kTAA) {
        JitterProjection(projection);
    }

    auto inverse_view = view.Inverted();
    auto inverse_projection = projection.Inverted();
    auto farz = camera->farz();
    auto nearz = camera->nearz();

    assert(inverse_view);
    assert(inverse_projection);

    param._View = view;
    param._InverseView = inverse_view.value();
    param._Projection = projection;
    param._InverseProjection = inverse_projection.value();
    param._ViewProjection = projection * view;
    param._InverseViewProjection = param._ViewProjection.Inverted().value();
    param._CameraParams = { nearz, farz, 1.0f / nearz, 1.0f / farz };

#ifdef GLACIER_REVERSE_Z
    param._ZBufferParams.x = -1.0f + farz / nearz;
    param._ZBufferParams.y = 1.0f;
    param._ZBufferParams.z = param._ZBufferParams.x / farz;
    param._ZBufferParams.w = 1.0f / farz;
#else
    param._ZBufferParams.x = 1.0f - farz / nearz;
    param._ZBufferParams.y = farz / nearz;
    param._ZBufferParams.z = param._ZBufferParams.x / farz;
    param._ZBufferParams.w = param._ZBufferParams.y / farz;
#endif

    param._DeltaTime = delta_time_;
    per_frame_param_.Update();
}

void Renderer::PreRender(CommandBuffer* cmd_buffer) {
    PerfSample("PreRender");
    stats_->PreRender(cmd_buffer);

    hdr_render_target_->Clear(cmd_buffer);

    UpdatePerFrameData();

    BindLightingTarget(cmd_buffer);
    BindMainCamera(cmd_buffer);
    FilterVisibles();

    //the passes draw the deformed vertices, so the skinning of the frame has to
    //be dispatched before the render graph runs
    {
        GpuPassScope pass(stats_->pass_timer(), "Skinning", cmd_buffer);
        RenderableManager::Instance()->UpdateSkinning(cmd_buffer);
    }
}

void Renderer::Render(float delta_time) {
    delta_time_ = delta_time;
    auto& state = Input::GetJustKeyDownState();
    auto cmd_buffer = gfx_->GetCommandQueue(CommandBufferType::kDirect)->GetCommandBuffer();

    PreRender(cmd_buffer);

    auto& mouse = Input::Instance()->mouse();
    if (mouse.IsLeftDown() && !mouse.IsRelativeMode()) {
        editor_.Pick(cmd_buffer, mouse.GetPosX(), mouse.GetPosY(), GetMainCamera(), visibles_, present_render_target_);
    }

    {
        // The passes that follow a runtime toggle rather than a value of the
        // frame are switched here, before the graph runs: the upsample of the
        // AO chain follows the half resolution option, and the outline chain
        // follows the selection of the editor (T2.2).
        if (gtao_upsample_pass_ != nullptr) {
            gtao_upsample_pass_->active(gtao_.half_resolution());
        }

        editor_.UpdatePassActivity();
    }

    {
        PerfSample("Exexute render graph");
        //the graph names its own passes, so the timer is handed to it rather
        //than bracketing each one here
        render_graph_.Execute(cmd_buffer, &stats_->pass_timer());
    }

    {
        GpuPassScope pass(stats_->pass_timer(), "Resolve MSAA", cmd_buffer);
        ResolveMSAA(cmd_buffer);
    }

    {
        GpuPassScope pass(stats_->pass_timer(), "TAA", cmd_buffer);
        DoTAA(cmd_buffer);
    }

    {
        GpuPassScope pass(stats_->pass_timer(), "Hdr Post Process", cmd_buffer);
        HdrPostProcess(cmd_buffer);
    }

    {
        // The meter of this frame has to run before the tone mapping reads the
        // exposure, or a frame is exposed with what the frame before it metered:
        // that leaves the first frame of a run - which has no frame before it -
        // exposed with whatever the exposure buffer was seeded with, and it is
        // the frame a cold start leaves on the screen the longest.
        GpuPassScope pass(stats_->pass_timer(), "Exposure", cmd_buffer);
        exposure_.UpdateExposure(cmd_buffer);
    }

    {
        PerfSample("Bloom");
        GpuPassScope pass(stats_->pass_timer(), "Bloom", cmd_buffer);
        bloom_.Execute(this, cmd_buffer);
    }

    {
        GpuPassScope pass(stats_->pass_timer(), "Tone Mapping", cmd_buffer);
        tonemapping_.Execute(this, cmd_buffer);
    }

    {
        PerfSample("Post Process");
        GpuPassScope pass(stats_->pass_timer(), "Post Process", cmd_buffer);
        LdrPostProcess(cmd_buffer);
        post_process_manager_.Render(cmd_buffer);
    }

    {
        GpuPassScope pass(stats_->pass_timer(), "FXAA", cmd_buffer);
        DoFXAA(cmd_buffer);
    }

    present_render_target_->Bind(cmd_buffer);

    {
        GpuPassScope pass(stats_->pass_timer(), "Editor", cmd_buffer);
        editor_.Render(cmd_buffer);
    }

    stats_->PostRender(cmd_buffer, editor_.ShowStats());

    gfx_->Present(cmd_buffer);

    PostRender(cmd_buffer);
}

void Renderer::HdrPostProcess(CommandBuffer* cmd_buffer) {
    exposure_.GenerateLuminance(cmd_buffer);
}

void Renderer::PostRender(CommandBuffer* cmd_buffer) {
    Gizmos::Instance()->Clear();

    auto& param = per_frame_param_.param();
    param._PrevViewProjection = prev_view_projection_;

    render_graph_.Reset();

    ++frame_count_;
}

void Renderer::DoFXAA(CommandBuffer* cmd_buffer) {
    cmd_buffer->CopyResource(ldr_render_target_->GetColorAttachment(AttachmentPoint::kColor0).get(),
        present_render_target_->GetColorAttachment(AttachmentPoint::kColor0).get());
}

void Renderer::CaptureShadowMap() {
    csm_manager_->CaptureShadowMap();
}

void Renderer::SetupBuiltinProperty(Material* mat) {
    if (mat->HasParameter("_PerFrameData")) {
        mat->SetProperty("_PerFrameData", per_frame_param_);
    }

    if (mat->HasParameter("_PerObjectData")) {
        mat->SetProperty("_PerObjectData", Renderable::GetPerObjectData());
    }

    gtao_.SetupBuiltinProperty(mat);

    csm_manager_->SetupMaterial(mat);
    LightManager::Instance()->SetupMaterial(mat);
}

void Renderer::CaptureScreen() {
    auto width = ldr_render_target_->width();
    auto height = ldr_render_target_->height();
    auto cmd_queue = gfx_->GetCommandQueue(CommandBufferType::kDirect);
    auto cmd_buffer = cmd_queue->GetCommandBuffer();

    auto& tex = ldr_render_target_->GetColorAttachment(AttachmentPoint::kColor0);
    tex->ReadBackImage(cmd_buffer, 0, 0, width, height, 0, 0,
        [width, height, this](const uint8_t* data, size_t raw_pitch) {
            Image image(width, height, false);

            for (int y = 0; y < height; ++y) {
                const uint8_t* texel = data;
                auto pixel = image.GetRawPtr<ColorRGBA32>(0, y);
                for (int x = 0; x < width; ++x) {
                    *pixel = *((const ColorRGBA32*)texel);
                    texel += sizeof(ColorRGBA32);
                    ++pixel;
                }
                data += raw_pitch;
            }

            image.Save(TEXT("ScreenCaptured.png"), false);
        });

    //the copy recorded above is only queued, it still has to be submitted
    cmd_queue->ExecuteCommandBuffer(cmd_buffer);
}

void Renderer::AddShadowPass(GraphResourceHandle shadow_map) {
    render_graph_.AddPass("shadow",
        [=](PassBuilder& b) {
            b.Write(shadow_map, ResourceAccessBit::kDepthWrite);
            // The pass puts the main camera and the lighting target back when
            // it is done - the reset its consumers rely on. The camera is not
            // a resource state; the target it leaves bound is, so it is part
            // of what the pass declared (T2.1). Both are the state the frame
            // starts in and the pass that follows the shadow expects anyway.
            b.Write(hdr_color_handle(), ResourceAccessBit::kRenderTarget);
            b.Write(hdr_depth_handle(), ResourceAccessBit::kDepthWrite);
        },
        [this](PassContext& ctx, const PassNode& pass) {
            auto cmd_buffer = ctx.cmd_buffer();
            PerfSample("Shadow pass");

            auto light_mgr = LightManager::Instance();
            auto main_camera_ = GetMainCamera();
            LightManager::Instance()->SortLight(main_camera_->ViewCenter());
            auto main_light = light_mgr->GetMainLight();
            if (!main_light || !main_light->HasShadow()) return;

            csm_manager_->Render(cmd_buffer, main_camera_, visibles_, main_light);

            BindMainCamera(cmd_buffer);
            BindLightingTarget(cmd_buffer);
        });
}

void Renderer::AddGtaoPass(GraphResourceHandle depth, GraphResourceHandle normal, GraphResourceHandle occlusion) {
    // The chain is four nodes of the graph (T2.2, D8): the trace, the upsample
    // that puts a half resolution trace back on the full target, and the two
    // directions of the bilateral filter. The target of the full resolution
    // trace and the source of the first filter direction is `occlusion` - the
    // full target the lighting pass reads, owned by the renderer; the other
    // two are transients of the graph (T2.4).
    render_graph_.AddPass("GTAO trace",
        [=](PassBuilder& b) {
            b.Read(depth, ResourceAccessBit::kPixelShaderRead);
            b.Read(normal, ResourceAccessBit::kPixelShaderRead);
            ao_half_ = b.CreateTexture("ao half texture",
                GraphTextureDesc::Scale(TextureScale::kHalf)
                    .Format(TextureFormat::kR11G11B10_FLOAT)
                    .CreateFlag(CreateFlags::kRenderTarget));
            ao_spatial_ = b.CreateTexture("ao spatial texture",
                GraphTextureDesc::Scale(TextureScale::kFull)
                    .Format(TextureFormat::kR11G11B10_FLOAT)
                    .CreateFlag(CreateFlags::kRenderTarget));
            b.Write(ao_half_, ResourceAccessBit::kRenderTarget);
            // the trace renders into the full target directly while it runs at
            // full resolution
            b.Write(occlusion, ResourceAccessBit::kRenderTarget);
        },
        [=](PassContext& ctx, const PassNode& pass) {
            gtao_.BindHalfTarget(ctx.GetTextureShared(ao_half_));
            gtao_.ExecuteTrace(this, ctx.cmd_buffer());
        });

    auto& upsample = render_graph_.AddPass("GTAO upsample",
        [=](PassBuilder& b) {
            b.Read(depth, ResourceAccessBit::kPixelShaderRead);
            b.Read(ao_half_, ResourceAccessBit::kPixelShaderRead);
            b.Write(occlusion, ResourceAccessBit::kRenderTarget);
        },
        [=](PassContext& ctx, const PassNode& pass) {
            gtao_.ExecuteUpsample(ctx.cmd_buffer());
        });

    // the upsample is part of the chain only while the trace runs at half
    // resolution (T2.2); a full resolution trace writes the full target and
    // the filter below reads it straight away. Renderer::Render switches it
    // with the state of the AO option before the graph runs.
    gtao_upsample_pass_ = &upsample;

    render_graph_.AddPass("GTAO filter x",
        [=](PassBuilder& b) {
            b.Read(depth, ResourceAccessBit::kPixelShaderRead);
            b.Read(occlusion, ResourceAccessBit::kPixelShaderRead);
            b.Write(ao_spatial_, ResourceAccessBit::kRenderTarget);
        },
        [=](PassContext& ctx, const PassNode& pass) {
            gtao_.BindSpatialTarget(ctx.GetTextureShared(ao_spatial_));
            gtao_.ExecuteFilterX(ctx.cmd_buffer());
        });

    render_graph_.AddPass("GTAO filter y",
        [=](PassBuilder& b) {
            b.Read(depth, ResourceAccessBit::kPixelShaderRead);
            b.Read(ao_spatial_, ResourceAccessBit::kPixelShaderRead);
            b.Write(occlusion, ResourceAccessBit::kRenderTarget);
        },
        [=](PassContext& ctx, const PassNode& pass) {
            gtao_.ExecuteFilterY(ctx.cmd_buffer());
        });
}

void Renderer::InitDefaultPbr(CommandBuffer* cmd_buffer) {
    auto pbr_mat = CreateLightingMaterial("pbr_default");

    pbr_mat->SetProperty("AlbedoTexture", nullptr, Color::kWhite);
    pbr_mat->SetProperty("MetalRoughnessTexture", nullptr, Color(0.0f, 0.5f, 0.0f, 1.0f));
    pbr_mat->SetProperty("AoTexture", nullptr, Color::kWhite);
    pbr_mat->SetProperty("EmissiveTexture", nullptr, Color::kBlack);

    struct PbrMaterial {
        Vec3f f0 = Vec3f(0.04f);
        uint32_t use_normal_map = 0;
    };

    PbrMaterial pbr_param;
    auto mat_cbuf = gfx_->CreateConstantBuffer<PbrMaterial>(pbr_param, UsageType::kDefault);
    pbr_mat->SetProperty("object_material", mat_cbuf);

    auto red_pbr = std::make_unique<Material>(*pbr_mat);
    red_pbr->name("red_pbr_default");
    red_pbr->SetProperty("AlbedoTexture", nullptr, Color::kRed);

    auto green_pbr = std::make_unique<Material>(*pbr_mat);
    green_pbr->name("green_pbr_default");
    green_pbr->SetProperty("AlbedoTexture", nullptr, Color::kGreen);

    auto orange_pbr = std::make_unique<Material>(*pbr_mat);
    orange_pbr->name("orange_pbr_default");
    orange_pbr->SetProperty("AlbedoTexture", nullptr, Color::kOrange);

    auto indigo_pbr = std::make_unique<Material>(*pbr_mat);
    indigo_pbr->name("indigo_pbr_default");
    indigo_pbr->SetProperty("AlbedoTexture", nullptr, Color::kIndigo);

    MaterialManager::Instance()->Add(std::move(pbr_mat));
    MaterialManager::Instance()->Add(std::move(red_pbr));
    MaterialManager::Instance()->Add(std::move(green_pbr));
    MaterialManager::Instance()->Add(std::move(orange_pbr));
    MaterialManager::Instance()->Add(std::move(indigo_pbr));
}

void Renderer::InitFloorPbr(CommandBuffer* cmd_buffer) {
    auto albedo_tex = cmd_buffer->CreateTextureFromFile(TEXT("assets\\textures\\floor_albedo.png"), true, true);
    auto normal_tex = cmd_buffer->CreateTextureFromFile(TEXT("assets\\textures\\floor_normal.png"), false, true);

    auto pbr_mat = CreateLightingMaterial("pbr_floor");
    pbr_mat->SetTexTilingOffset({ 5.0f, 5.0f, 0.0f, 0.0f });

    pbr_mat->SetProperty("AlbedoTexture", albedo_tex);
    pbr_mat->SetProperty("NormalTexture", normal_tex);
    pbr_mat->SetProperty("MetalRoughnessTexture", nullptr, Color(0.0f, 0.5f, 0.0f, 1.0f));
    pbr_mat->SetProperty("AoTexture", nullptr, Color::kWhite);
    pbr_mat->SetProperty("EmissiveTexture", nullptr, Color::kBlack);

    struct PbrMaterial {
        Vec3f f0 = Vec3f(0.04f);
        uint32_t use_normal_map = 1;
    };

    PbrMaterial pbr_param;
    auto mat_cbuf = gfx_->CreateConstantBuffer<PbrMaterial>(pbr_param, UsageType::kDefault);
    pbr_mat->SetProperty("object_material", mat_cbuf);

    MaterialManager::Instance()->Add(std::move(pbr_mat));
}

void Renderer::OptionWindow(bool* open) {
    auto width = present_render_target_->width();
    auto height = present_render_target_->height();
    ImGui::SetNextWindowPos(ImVec2(width * 0.3f, height * 0.3f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags window_flags = 0;// ImGuiWindowFlags_AlwaysAutoResize;
    if (!ImGui::Begin("Renderer Option", open, window_flags)) {
        // Early out if the window is collapsed, as an optimization.
        ImGui::End();
        return;
    }

    DrawOptionWindow();

    bloom_.DrawOptionWindow();
    gtao_.DrawOptionWindow();
    exposure_.DrawOptionWindow();
    csm_manager_->DrawOptionWindow();

    ImGui::End();
}

}
}
