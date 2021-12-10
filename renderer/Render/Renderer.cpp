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
#include "Render/Image.h"
#include "Render/Base/Renderable.h"
#include "Render/Base/RenderTarget.h"
#include "Render/Base/Sampler.h"
#include "Render/Base/ConstantBuffer.h"
#include "Render/Base/SwapChain.h"

namespace glacier {
namespace render {

Renderer::Renderer(GfxDriver* gfx) : gfx_(gfx), editor_(gfx), post_process_manager_(this) {
    Gizmos::Instance()->Setup(gfx);

    stats_ = std::make_unique<PerfStats>(gfx);

    csm_manager_ = std::make_shared<CascadedShadowManager>(gfx, 1024, std::vector<float>{ 15, 40, 70, 100 });

    UpdateRenderTarget(gfx_);
    InitMaterial(gfx_);
    
    auto tonemapping_mat = std::make_shared<PostProcessMaterial>("tone mapping", TEXT("ToneMapping"));
    tonemapping_mat->SetProperty("sam", post_process_manager_.GetSampler());
    
    auto builder = PostProcess::Builder()
        .SetSrc(render_target_->GetColorAttachment(AttachmentPoint::kColor0))
        .SetDst(gfx->GetSwapChain()->GetRenderTarget())
        .SetMaterial(tonemapping_mat);

    post_process_manager_.Push(builder);
}

void Renderer::Setup() {
    LightManager::Instance()->Setup(gfx_);
}

void Renderer::OnResize(uint32_t width, uint32_t height) {
    auto swapchain = gfx_->GetSwapChain();
    swapchain->OnResize(width, height);

    auto depth_tex = swapchain->GetRenderTarget()->GetDepthStencil();
    auto rt_tex = render_target_->GetColorAttachment(AttachmentPoint::kColor0);
    rt_tex->Resize(width, height);

    render_target_->Resize(width, height);
    render_target_->AttachColor(AttachmentPoint::kColor0, rt_tex);
    render_target_->AttachDepthStencil(depth_tex);
}

void Renderer::FilterVisibles() {
    auto main_camera = GetMainCamera();

    visibles_.clear();
    main_camera->Cull(RenderableManager::Instance()->GetList(), visibles_);

    //TODO: sort by depth?
    std::sort(visibles_.begin(), visibles_.end(), [](Renderable* a, Renderable* b) {
        return a->GetMaterial()->id() < b->GetMaterial()->id();
        });
}

void Renderer::RestoreCommonBindings() {
    auto main_camera = GetMainCamera();
    gfx_->BindCamera(main_camera);
    render_target_->Bind();
}

void Renderer::InitMaterial(GfxDriver* gfx) {
    auto solid_program = gfx->CreateProgram("Solid", TEXT("Solid"), TEXT("Solid"));

    auto solid_mat = MaterialManager::Instance()->Create("solid");
    solid_mat->SetProgram(solid_program);
    solid_mat->SetProperty("color", Color{ 1.0f,1.0f,1.0f, 1.0f });
}

void Renderer::UpdateRenderTarget(GfxDriver* gfx) {
    auto width = gfx->width();
    auto height = gfx->height();

    auto& swapchain_render_target = gfx->GetSwapChain()->GetRenderTarget();
    auto depthstencil_texture = swapchain_render_target->GetDepthStencil();

    auto color0_builder = Texture::Builder()
        .SetDimension(width, height)
        .SetFormat(TextureFormat::kR16G16B16A16_FLOAT);
    auto color0_tex = gfx->CreateTexture(color0_builder);

    render_target_ = gfx->CreateRenderTarget(width, height);
    render_target_->AttachColor(AttachmentPoint::kColor0, color0_tex);
    render_target_->AttachDepthStencil(depthstencil_texture);
}

Camera* Renderer::GetMainCamera() const {
    auto scene = SceneManager::Instance()->CurrentScene();
    if (!scene) {
        return nullptr;
    }

    return scene->GetMainCamera();
}

void Renderer::PreRender() {
    stats_->PreRender();
    gfx_->BeginFrame();

    render_target_->Clear();
    RestoreCommonBindings();
    FilterVisibles();
}

void Renderer::PostRender() {
    Gizmos::Instance()->Clear();

    // present
    gfx_->EndFrame();

    render_graph_.Reset();
}

void Renderer::Render() {
    auto& present_render_target = gfx_->GetSwapChain()->GetRenderTarget();
    PreRender();

    auto& mouse = Input::Instance()->mouse();
    if (mouse.IsLeftDown() && !mouse.IsRelativeMode()) {
        editor_.Pick(mouse.GetPosX(), mouse.GetPosY(), GetMainCamera(), visibles_, present_render_target);
    }

    render_graph_.Execute(this);
    render_target_->UnBind();
    
    post_process_manager_.Render();

    present_render_target->Bind();

    physics::World::Instance()->OnDrawGizmos(true);
    //auto sel = editor_.GetSelected();
    //if (sel) {
    //    auto mesh_renderer = sel->GetComponent<MeshRenderer>();
    //    if (mesh_renderer) {
    //        Gizmos::Instance()->DrawWireMesh(*mesh_renderer);
    //    }
    //}

    auto& state = Input::GetJustKeyDownState();
    if (state.Tab) {
        show_gui_ = !show_gui_;
    }

    if (show_gui_) {
        editor_.DoFrame();
    }

    if (state.F4) {
        show_gizmo_ = !show_gizmo_;
    }

    if (show_gizmo_) {
        Gizmos::Instance()->Render(gfx_);
    }

    if (state.F1) {
        show_imgui_demo_ = !show_imgui_demo_;
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

void Renderer::GrabScreen() {
    auto render_target = gfx_->GetSwapChain()->GetRenderTarget();
    Image tmp_img(render_target->width(), render_target->height());

    auto& tex = render_target->GetColorAttachment(AttachmentPoint::kColor0);
    tex->ReadBackImage(tmp_img, 0, 0, render_target->width(), render_target->height(), 0, 0);
    tmp_img.Save(TEXT("screen_grab.png"), false);
}

void Renderer::AddPhongPass() {
    render_graph_.AddPass("phong",
        [&](PassNode& pass) {
        },
        [this](Renderer* renderer, const PassNode& pass) {
            auto gfx = renderer->driver();

            ResourceGuard<LightManager> light_gurad(LightManager::Instance());
            ResourceGuard<CascadedShadowManager> csm_guard(csm_manager_.get());

            pass.Render(renderer, visibles_);
        });
}

void Renderer::AddPbrPass() {
    render_graph_.AddPass("pbr",
        [&](PassNode& pass) {
        },
        [this](Renderer* renderer, const PassNode& pass) {
            auto gfx = renderer->driver();

            ResourceGuard<LightManager> light_gurad(LightManager::Instance());
            ResourceGuard<CascadedShadowManager> csm_guard(csm_manager_.get());
            
            pass.Render(renderer, visibles_);
        });
}

void Renderer::AddSolidPass() {
    auto mat = MaterialManager::Instance()->Get("solid");
    render_graph_.AddPass("solid",
        [&](PassNode& pass) {
        },
        [this, mat](Renderer* renderer, const PassNode& pass) {
            MaterialGuard guard(renderer->driver(), mat);
            pass.Render(renderer, visibles_);
        });
}

void Renderer::AddShadowPass() {
    render_graph_.AddPass("shadow",
        [&](PassNode& pass) {
        },
        [this](Renderer* renderer, const PassNode& pass) {
            auto light_mgr = LightManager::Instance();
            auto main_camera_ = GetMainCamera();
            LightManager::Instance()->SortLight(main_camera_->ViewCenter());
            auto main_light = light_mgr->GetMainLight();
            if (!main_light || !main_light->HasShadow()) return;

            pass.PreRender(renderer);
            csm_manager_->Render(main_camera_, visibles_, main_light);
            pass.PostRender(renderer);

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

    auto builder1 = Texture::Builder()
        .SetType(TextureType::kTextureCube)
        .SetDimension(size, size)
        .SetFormat(TextureFormat::kR32_FLOAT);

    auto shadow_map_cube = gfx->CreateTexture(builder1);

    auto builder2 = Texture::Builder()
        .SetDimension(size, size)
        .SetFormat(TextureFormat::kR24G8_TYPELESS)
        .SetCreateFlag(D3D11_BIND_DEPTH_STENCIL);
    auto depthstencil_texture1 = gfx->CreateTexture(builder2);

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

            pass.PreRender(renderer);
            //auto shadow_cubemap
            for (size_t i = 0; i < 6; i++)
            {
                auto& rt = shadow_map_rt[i];
                rt->Clear();
                rt->Bind();

                auto camera = shadow_cameras[i];
                camera->position({ light_pos.x, light_pos.y, light_pos.z });
                gfx->BindCamera(camera);

                MaterialGuard guard(gfx, shadow_mat_ptr);
                for (auto o : visibles_) {
                    o->Render(gfx);
                }
            }

            auto& rt = shadow_map_rt[5];
            //for binding as PSV, tex can't be render target simultaneously
            rt->UnBind();
            pass.PostRender(renderer);

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

    auto sampler1 = gfx->CreateSampler(ss);

    SamplerState ss1;
    ss1.warpU = ss1.warpV = WarpMode::kClamp;
    ss.filter = FilterMode::kAnisotropic;
    
    auto sampler2 = gfx->CreateSampler(ss1);

    /// FIXME
    auto phong_mat = std::make_unique<Material>("lambert_cube");
    phong_mat->SetProperty("shadow_trans", shadow_space_tx);
    phong_mat->SetProperty("shadow_map", shadow_map_cube);
    phong_mat->SetProperty("sampler1", sampler1);
    phong_mat->SetProperty("sampler2", sampler2);

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
