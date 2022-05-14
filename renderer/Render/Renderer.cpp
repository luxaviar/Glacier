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

Renderer::Renderer(GfxDriver* gfx) :
    gfx_(gfx),
    editor_(gfx),
    post_process_manager_(gfx)
{
    stats_ = std::make_unique<PerfStats>(gfx);
    csm_manager_ = std::make_shared<CascadedShadowManager>(gfx, 1024, std::vector<float>{ 15, 40, 70, 100 });
}

void Renderer::Setup() {
    if (init_) return;
    init_ = true;

    InitRenderTarget();
    InitMaterial();

    Gizmos::Instance()->Setup(gfx_);
    LightManager::Instance()->Setup(gfx_, this);
}

void Renderer::InitRenderTarget() {
    auto width = gfx_->GetSwapChain()->GetWidth();
    auto height = gfx_->GetSwapChain()->GetHeight();
    present_render_target_ = gfx_->GetSwapChain()->GetRenderTarget();
    auto backbuffer_depth_tex = present_render_target_->GetDepthStencil();

    auto hdr_colorframe = RenderTexturePool::Get(width, height, TextureFormat::kR16G16B16A16_FLOAT,
        CreateFlags::kRenderTarget);// , sample_count_, quality_level_);
    hdr_colorframe->SetName(TEXT("hdr color frame"));

    hdr_render_target_ = gfx_->CreateRenderTarget(width, height);
    hdr_render_target_->AttachColor(AttachmentPoint::kColor0, hdr_colorframe);
    hdr_render_target_->AttachDepthStencil(backbuffer_depth_tex);

    ldr_render_target_ = gfx_->CreateRenderTarget(width, height);
    auto ldr_colorframe = RenderTexturePool::Get(width, height);
    ldr_colorframe->SetName(TEXT("ldr color frame"));
    ldr_render_target_->AttachColor(AttachmentPoint::kColor0, ldr_colorframe);
    ldr_render_target_->AttachDepthStencil(backbuffer_depth_tex);
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

    return true;
}

void Renderer::DoToneMapping() {
    tonemapping_mat_->SetProperty("PostSourceTexture_", hdr_render_target_->GetColorAttachment(AttachmentPoint::kColor0));
    PostProcess(ldr_render_target_, tonemapping_mat_.get());
}

void Renderer::FilterVisibles() {
    PerfSample("Filter Visibles");
    auto main_camera = GetMainCamera();

    visibles_.clear();
    main_camera->Cull(RenderableManager::Instance()->GetList(), visibles_);

    //TODO: sort by depth?
    std::sort(visibles_.begin(), visibles_.end(), [](Renderable* a, Renderable* b) {
        auto template_a = a->GetMaterial()->GetTemplate()->id();
        auto template_b = b->GetMaterial()->GetTemplate()->id();
        if (template_a == template_b) {
            return a->GetMaterial()->id() < b->GetMaterial()->id();
        }

        return template_a < template_b;
    });
}

void Renderer::RestoreCommonBindings() {
    auto main_camera = GetMainCamera();
    gfx_->BindCamera(main_camera);
    GetLightingRenderTarget()->Bind(gfx_);
}

void Renderer::InitMaterial() {
    auto solid_mat = std::make_unique<Material>("solid", TEXT("Solid"), TEXT("Solid"));
    solid_mat->SetProperty("color", Color{ 1.0f,1.0f,1.0f, 1.0f });
    solid_mat->GetTemplate()->SetInputLayout(InputLayoutDesc{ InputLayoutDesc::Position3D });

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

void Renderer::PreRender() {
    PerfSample("PreRender");
    stats_->PreRender();

    hdr_render_target_->Clear();

    RestoreCommonBindings();
    FilterVisibles();
}

void Renderer::PostRender() {
    Gizmos::Instance()->Clear();

    // present
    //gfx_->EndFrame();

    render_graph_.Reset();
}

void Renderer::Render() {
    auto& state = Input::GetJustKeyDownState();
    if (state.Tab) {
        show_gui_ = !show_gui_;
    }

    if (state.F4) {
        show_gizmo_ = !show_gizmo_;
    }

    if (state.F1) {
        show_imgui_demo_ = !show_imgui_demo_;
    }

    PreRender();

    auto& mouse = Input::Instance()->mouse();
    if (mouse.IsLeftDown() && !mouse.IsRelativeMode()) {
        editor_.Pick(mouse.GetPosX(), mouse.GetPosY(), GetMainCamera(), visibles_, present_render_target_);
    }

    {
        PerfSample("Exexute render graph");
        render_graph_.Execute(this);
    }

    hdr_render_target_->UnBind(gfx_);

    ResolveMSAA();

    DoToneMapping();
    
    {
        PerfSample("Post Process");
        post_process_manager_.Render();
    }

    DoFXAA();

    present_render_target_->Bind(gfx_);

    if (show_gizmo_) {
        PerfSample("Draw Gizmos");
        physics::World::Instance()->OnDrawGizmos(true);
        editor_.DrawGizmos();
        Gizmos::Instance()->Render(gfx_);
    }

    if (show_gui_) {
        PerfSample("Editor");
        editor_.DrawPanel();
    }

    if (show_imgui_demo_) {
        ImGui::ShowDemoWindow(&show_imgui_demo_);
    }

    if (state.F2) {
        GrabScreen();
    }

    if (state.F3) {
        csm_manager_->GrabShadowMap();
    }

    stats_->PostRender();

    gfx_->Present();

    PostRender();
}

void Renderer::DoFXAA() {
    gfx_->CopyResource(ldr_render_target_->GetColorAttachment(AttachmentPoint::kColor0),
        present_render_target_->GetColorAttachment(AttachmentPoint::kColor0));
}

void Renderer::GrabScreen() {
    auto render_target = gfx_->GetSwapChain()->GetRenderTarget();
    auto width = render_target->width();
    auto height = render_target->height();

    auto& tex = render_target->GetColorAttachment(AttachmentPoint::kColor0);
    tex->ReadBackImage(0, 0, render_target->width(), render_target->height(), 0, 0,
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

            image.Save(TEXT("screen_grab.png"), false);
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

            renderer->RestoreCommonBindings();
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

                //MaterialGuard guard(gfx, shadow_mat_ptr);
                for (auto o : visibles_) {
                    o->Render(gfx, shadow_mat_ptr);
                }
            }

            auto& rt = shadow_map_rt[5];
            //for binding as PSV, tex can't be render target simultaneously
            rt->UnBind(gfx);

            renderer->RestoreCommonBindings();
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
    phong_mat->GetTemplate()->SetProperty("sampler1", ss);
    phong_mat->GetTemplate()->SetProperty("sampler2", ss1);
    phong_mat->GetTemplate()->SetInputLayout(Mesh::kDefaultLayout);

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

}
}
