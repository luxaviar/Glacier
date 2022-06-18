#include "Renderer.h"
#include <memory>
#include <assert.h>
#include <algorithm>
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui.h"
#include "Math/Util.h"
#include "Render/Graph/PassNode.h"
#include "Render/Graph/ResourceEntry.h"
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

namespace glacier {
namespace render {

void Renderer::PostProcess(const std::shared_ptr<RenderTarget>& dst, Material* mat) {
    auto gfx = GfxDriver::Get();
    RenderTargetBindingGuard rt_guard(gfx, dst.get());
    MaterialGuard mat_guard(gfx, mat);

    gfx->Draw(3, 0);
}

Renderer::Renderer(GfxDriver* gfx, AntiAliasingType aa) :
    gfx_(gfx),
    anti_aliasing_(aa),
    editor_(gfx),
    post_process_manager_(gfx)
{
    per_frame_param_ = gfx->CreateConstantParameter<PerFrameData, UsageType::kDynamic>(nullptr);
    stats_ = std::make_unique<PerfStats>(gfx);
    csm_manager_ = std::make_shared<CascadedShadowManager>(gfx, 1024, std::vector<float>{ 10.6, 18.6, 19.2, 51.6 });

    for (int i = 0; i < kTAASampleCount; ++i) {
        halton_sequence_[i] = { LowDiscrepancySequence::Halton(i + 1, 2), LowDiscrepancySequence::Halton(i + 1, 3) };
        halton_sequence_[i] = halton_sequence_[i] * 2.0f - 1.0f;
        LOG_LOG("{}", halton_sequence_[i]);
    }
}

void Renderer::Setup() {
    if (init_) return;
    init_ = true;

    InitRenderTarget();
    InitMaterial();

    Gizmos::Instance()->Setup(gfx_);
    LightManager::Instance()->Setup(gfx_, this);

    InitFloorPbr(gfx_);
    InitDefaultPbr(gfx_);
}

void Renderer::InitRenderTarget() {
    auto width = gfx_->GetSwapChain()->GetWidth();
    auto height = gfx_->GetSwapChain()->GetHeight();
    present_render_target_ = gfx_->GetSwapChain()->GetRenderTarget();
    auto backbuffer_depth_tex = present_render_target_->GetDepthStencil();

    auto hdr_colorframe = RenderTexturePool::Get(width, height, TextureFormat::kR16G16B16A16_FLOAT,
        CreateFlags::kRenderTarget);
    hdr_colorframe->SetName(TEXT("hdr color frame"));

    hdr_render_target_ = gfx_->CreateRenderTarget(width, height);
    hdr_render_target_->AttachColor(AttachmentPoint::kColor0, hdr_colorframe);
    hdr_render_target_->AttachDepthStencil(backbuffer_depth_tex);

    hdr_color_target_ = gfx_->CreateRenderTarget(width, height);
    hdr_color_target_->AttachColor(AttachmentPoint::kColor0, hdr_render_target_->GetColorAttachment(AttachmentPoint::kColor0));

    ldr_render_target_ = gfx_->CreateRenderTarget(width, height);
    auto ldr_colorframe = RenderTexturePool::Get(width, height);
    ldr_colorframe->SetName(TEXT("ldr color frame"));
    ldr_render_target_->AttachColor(AttachmentPoint::kColor0, ldr_colorframe);
    ldr_render_target_->AttachDepthStencil(backbuffer_depth_tex);

    per_frame_param_.param()._ScreenParam = { (float)width, (float)height, 1.0f / (float)width, 1.0f / (float)height };
}

bool Renderer::OnResize(uint32_t width, uint32_t height) {
    auto swapchain = gfx_->GetSwapChain();
    if (swapchain->GetWidth() == width && swapchain->GetHeight() == height) {
        return false;
    }

    swapchain->OnResize(width, height);

    hdr_render_target_->Resize(width, height);
    hdr_color_target_->Resize(width, height);
    ldr_render_target_->Resize(width, height);

    editor_.OnResize(width, height);

    per_frame_param_.param()._ScreenParam = { (float)width, (float)height, 1.0f / (float)width, 1.0f / (float)height };

    return true;
}

void Renderer::DoToneMapping() {
    tonemapping_mat_->SetProperty("_PostSourceTexture", hdr_render_target_->GetColorAttachment(AttachmentPoint::kColor0));
    PostProcess(ldr_render_target_, tonemapping_mat_.get());
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

void Renderer::BindLightingTarget() {
    auto main_camera = GetMainCamera();
    gfx_->BindCamera(main_camera);
    GetLightingRenderTarget()->Bind(gfx_);
}

void Renderer::InitMaterial() {
    auto solid_mat = std::make_unique<Material>("solid", TEXT("Solid"), TEXT("Solid"));
    solid_mat->SetProperty("color", Color{ 1.0f,1.0f,1.0f, 1.0f });
    solid_mat->GetProgram()->SetInputLayout(InputLayoutDesc{ InputLayoutDesc::Position3D });

    MaterialManager::Instance()->Add(std::move(solid_mat));

    tonemapping_mat_ = std::make_shared<PostProcessMaterial>("tone mapping", TEXT("ToneMapping"));
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

    if (anti_aliasing_ == AntiAliasingType::kTAA) {
        uint64_t idx = frame_count_ % kTAASampleCount;
        double jitter_x = halton_sequence_[idx].x * (double)param._ScreenParam.z;
        double jitter_y = halton_sequence_[idx].y * (double)param._ScreenParam.w;
        projection[0][2] += jitter_x;
        projection[1][2] += jitter_y;
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
    param._CameraParams = { farz, nearz, 1.0f / farz, 1.0f / nearz };
    param._ZBufferParams.x = 1.0f - farz / nearz;
    param._ZBufferParams.y = 1.0f + farz / nearz;
    param._ZBufferParams.z = param._ZBufferParams.x / farz;
    param._ZBufferParams.w = param._ZBufferParams.y / farz;

    per_frame_param_.Update();
}

void Renderer::PreRender() {
    PerfSample("PreRender");
    stats_->PreRender();

    hdr_render_target_->Clear();

    UpdatePerFrameData();

    BindLightingTarget();
    FilterVisibles();
}

void Renderer::PostRender() {
    Gizmos::Instance()->Clear();

    auto& param = per_frame_param_.param();
    param._PrevViewProjection = prev_view_projection_;

    render_graph_.Reset();

    ++frame_count_;
}

void Renderer::Render() {
    auto& state = Input::GetJustKeyDownState();

    PreRender();

    auto& mouse = Input::Instance()->mouse();
    if (mouse.IsLeftDown() && !mouse.IsRelativeMode()) {
        editor_.Pick(mouse.GetPosX(), mouse.GetPosY(), GetMainCamera(), visibles_, present_render_target_);
    }

    {
        PerfSample("Exexute render graph");
        render_graph_.Execute(this);
    }

    ResolveMSAA();

    DoTAA();

    HdrPostProcess();

    DoToneMapping();
    
    {
        PerfSample("Post Process");
        LdrPostProcess();
        post_process_manager_.Render();
    }

    DoFXAA();

    present_render_target_->Bind(gfx_);

    editor_.Render();

    stats_->PostRender(editor_.ShowStats());

    gfx_->Present();

    PostRender();
}

void Renderer::DoFXAA() {
    gfx_->CopyResource(ldr_render_target_->GetColorAttachment(AttachmentPoint::kColor0),
        present_render_target_->GetColorAttachment(AttachmentPoint::kColor0));
}

void Renderer::CaptureShadowMap() {
    csm_manager_->CaptureShadowMap();
}

void Renderer::SetupBuiltinProperty(Material* mat) {
    if (mat->HasParameter("_PerFrameData")) {
        mat->SetProperty("_PerFrameData", per_frame_param_);
    }

    if (mat->HasParameter("_PerObjectData")) {
        mat->SetProperty("_PerObjectData", Renderable::GetTransformCBuffer(gfx_));
    }

    csm_manager_->SetupMaterial(mat);
    LightManager::Instance()->SetupMaterial(mat);
}

void Renderer::CaptureScreen() {
    auto width = ldr_render_target_->width();
    auto height = ldr_render_target_->height();

    auto& tex = ldr_render_target_->GetColorAttachment(AttachmentPoint::kColor0);
    tex->ReadBackImage(0, 0, width, height, 0, 0,
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
}

void Renderer::AddShadowPass() {
    render_graph_.AddPass("shadow",
        [&](PassNode& pass) {
        },
        [this](Renderer* renderer, const PassNode& pass) {
            PerfSample("Shadow pass");

            auto light_mgr = LightManager::Instance();
            auto main_camera_ = GetMainCamera();
            LightManager::Instance()->SortLight(main_camera_->ViewCenter());
            auto main_light = light_mgr->GetMainLight();
            if (!main_light || !main_light->HasShadow()) return;

            csm_manager_->Render(main_camera_, visibles_, main_light);

            renderer->BindLightingTarget();
        });
}

void Renderer::AddCubeShadowMap(GfxDriver* gfx, OldPointLight& light) {
    static constexpr UINT size = 1000;
    std::array<std::tuple<Vec3f, Vec3f>, 6> param = { {
        // +x
        {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        // -x
        {{-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        // +y
        {{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        // -y
        {{0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        // +z
        {{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
        // -z
        {{0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},
    }};

    std::array<Camera*, 6> shadow_cameras;
    for (int i = 0; i < 6; ++i) {
        auto& go = GameObject::Create("cube map shadow camera");
        go.DontDestroyOnLoad(true);
        auto camera = go.AddComponent<Camera>(CameraType::kPersp);
        camera->LookAt(std::get<0>(param[i]), std::get<1>(param[i]));
        camera->fov(90.0f);
        camera->aspect(1.0f);
        shadow_cameras[i] = camera;
    }

    auto desc1 = Texture::Description()
        .SetType(TextureType::kTextureCube)
        .SetDimension(size, size)
        .SetFormat(TextureFormat::kR32_FLOAT);

    auto shadow_map_cube = gfx->CreateTexture(desc1);
    shadow_map_cube->SetName(TEXT("shadow_map_cube texture"));

    auto desc2 = Texture::Description()
        .SetDimension(size, size)
        .SetFormat(TextureFormat::kR24G8_TYPELESS)
        .SetCreateFlag(CreateFlags::kDepthStencil)
        .SetCreateFlag(CreateFlags::kShaderResource);
    auto depthstencil_texture1 = gfx->CreateTexture(desc2);
    depthstencil_texture1->SetName(TEXT("shadow_map_cube depth texture"));

    auto shadow_mat = std::make_unique<Material>("cube shadow", TEXT("Shadow"), TEXT("Shadow"));

    auto shadow_mat_ptr = shadow_mat.get();
    MaterialManager::Instance()->Add(std::move(shadow_mat));

    render_graph_.AddPass<std::vector<std::shared_ptr<RenderTarget>>>("shadow",
        [&](std::vector<std::shared_ptr<RenderTarget>>& shadow_map_rt, PassNode& pass) {
            shadow_map_rt.reserve(6);

            for (int i = 0; i < 6; ++i) {
                auto rt = gfx->CreateRenderTarget(shadow_map_cube->width(), shadow_map_cube->height());
                shadow_map_rt.push_back(rt);
                rt->AttachColor(AttachmentPoint::kColor0, shadow_map_cube, i);
                rt->AttachDepthStencil(depthstencil_texture1);
            }
        },
        [&](Renderer* renderer, std::vector<std::shared_ptr<RenderTarget>>& shadow_map_rt, PassNode& pass) {
            auto light_pos = light.pos();
            auto gfx = renderer->driver();

            //auto shadow_cubemap
            for (size_t i = 0; i < 6; i++)
            {
                auto& rt = shadow_map_rt[i];
                rt->Clear();
                rt->Bind(gfx);

                auto camera = shadow_cameras[i];
                camera->position({ light_pos.x, light_pos.y, light_pos.z });
                gfx->BindCamera(camera);

                for (auto o : visibles_) {
                    o->Render(gfx, shadow_mat_ptr);
                }
            }

            auto& rt = shadow_map_rt[5];
            //for binding as PSV, tex can't be render target simultaneously
            rt->UnBind(gfx);

            renderer->BindLightingTarget();
        });

    render_graph_.AddPass("solid",
        [&](PassNode& pass) {
        },
        [this](Renderer* renderer, const PassNode& pass) {
            pass.Render(renderer, visibles_);
        });

    auto shadow_space_tx = gfx->CreateConstantBuffer<Matrix4x4>();

    SamplerState ss;
    ss.warpU = ss.warpV = WarpMode::kBorder;
    ss.filter = FilterMode::kCmpBilinear;
    ss.comp = CompareFunc::kLessEqual;

    SamplerState ss1;
    ss1.warpU = ss1.warpV = WarpMode::kClamp;
    ss.filter = FilterMode::kAnisotropic;

    /// FIXME
    auto phong_mat = std::make_unique<Material>("lambert_cube");
    phong_mat->SetProperty("shadow_trans", shadow_space_tx);
    phong_mat->SetProperty("shadow_map", shadow_map_cube);
    phong_mat->GetProgram()->SetProperty("sampler1", ss);
    phong_mat->GetProgram()->SetProperty("sampler2", ss1);
    phong_mat->GetProgram()->SetInputLayout(Mesh::kDefaultLayout);

    auto phong_mat_ptr = phong_mat.get();
    MaterialManager::Instance()->Add(std::move(phong_mat));

    render_graph_.AddPass("phong",
        [&](PassNode& pass) {
        },
        [&](Renderer* renderer, const PassNode& pass) {
            //TODO: gather light list
            auto main_camera_ = GetMainCamera();
            light.Bind(main_camera_->view());

            //transform matrix: world -> shadow
            auto light_pos = light.pos();
            auto t = Matrix4x4::Translate({ -light_pos.x, -light_pos.y, -light_pos.z });
            shadow_space_tx->Update(&t);

            MaterialGuard guard(renderer->driver(), phong_mat_ptr);
            pass.Render(renderer, visibles_);
        });
}

void Renderer::InitDefaultPbr(GfxDriver* gfx) {
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
    auto mat_cbuf = gfx->CreateConstantBuffer<PbrMaterial>(pbr_param, UsageType::kDefault);
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

void Renderer::InitFloorPbr(GfxDriver* gfx) {
    auto albedo_desc = Texture::Description()
        .SetFile(TEXT("assets\\textures\\floor_albedo.png"))
        .EnableSRGB()
        .EnableMips();
    auto albedo_tex = gfx->CreateTexture(albedo_desc);

    auto normal_desc = Texture::Description()
        .SetFile(TEXT("assets\\textures\\floor_normal.png"));
    auto normal_tex = gfx->CreateTexture(normal_desc);

    // auto metal_desc = Texture::Description()
    //     .SetFile(TEXT("assets\\textures\\floor_metallic.png"))
    //     .EnableMips();
    // auto metal_roughness_tex = gfx->CreateTexture(metal_desc);

    // auto ao_desc = Texture::Description()
    //     .SetFile(TEXT("assets\\textures\\floor_roughness.png"))
    //     .EnableMips()
    //     .EnableSRGB();
    // auto ao_tex = gfx->CreateTexture(ao_desc);

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
    auto mat_cbuf = gfx->CreateConstantBuffer<PbrMaterial>(pbr_param, UsageType::kDefault);
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

    csm_manager_->DrawOptionWindow();

    ImGui::End();
}

}
}
