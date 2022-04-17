#include "Editor.h"
#include <map>
#include "imgui/imgui.h"
#include "Core/GameObject.h"
#include "Core/ObjectManager.h"
#include "Render/Mesh/MeshRenderer.h"
#include "Render/Graph/PassNode.h"
#include "Render/Graph/RenderGraph.h"
#include "Render/Renderer.h"
#include "Physics/World.h"
#include "Render/Base/RenderTarget.h"
#include "Render/Base/Buffer.h"
#include "Render/Base/SamplerState.h"
#include "Render/Base/SwapChain.h"
#include "../Image.h"

namespace glacier {
namespace render {

Editor::Editor(GfxDriver* gfx) :
    gfx_(gfx),
    width_(gfx->GetSwapChain()->GetWidth()),
    height_(gfx->GetSwapChain()->GetHeight())
{
    color_buf_ = gfx->CreateConstantBuffer<Vec4f>();

    RasterStateDesc rs;
    rs.scissor = true;

    mat_ = std::make_shared<Material>("pick", TEXT("Solid"), TEXT("Solid"));
    mat_->GetTemplate()->SetRasterState(rs);
    mat_->GetTemplate()->SetInputLayout(Mesh::kDefaultLayout);
    mat_->SetProperty("color", color_buf_);
    mat_->SetProperty("object_transform", Renderable::GetTransformCBuffer(gfx_));
}

void Editor::Pick(int x, int y, Camera* camera, const std::vector<Renderable*>& visibles, std::shared_ptr<RenderTarget>& rt) {
    if (visibles.empty()) return;

    auto viewport = rt->viewport();
    Vec2i size{ 2, 2 };

    int minx = math::Round(x - size.x / 2.0f);
    int miny = math::Round(y - size.y / 2.0f);
    int maxx = math::Round(x + size.x / 2.0f);
    int maxy = math::Round(y + size.y / 2.0f);

    minx = std::max(minx, (int)viewport.top_left_x);
    miny = std::max(miny, (int)viewport.top_left_y);
    maxx = std::min(maxx, (int)(viewport.top_left_x + viewport.width - 1));
    maxy = std::min(maxy, (int)(viewport.top_left_y + viewport.height - 1));

    int sizex = maxx - minx;
    int sizey = maxy - miny;
    if (sizex <= 0 || sizey <= 0) return;

    ScissorRect rect{ minx, miny, maxx, maxy };
    rt->EnableScissor(rect);
    rt->Clear({ 0, 0, 0, 0 });
    rt->Bind(gfx_);

    gfx_->BindCamera(camera);
    {
        Vec4f encoded_id;
        for (auto o : visibles) {
            if (!o->IsActive() || !o->IsPickable()) continue;

            auto id = o->id();
            //a b g r uint32_t
            encoded_id.r = (id & 0xFF) / 255.0f;
            encoded_id.g = ((id >> 8) & 0xFF) / 255.0f;
            encoded_id.b = ((id >> 16) & 0xFF) / 255.0f;
            encoded_id.a = ((id >> 24) & 0xFF) / 255.0f;
            color_buf_->Update(&encoded_id);

            o->Render(gfx_, mat_.get());
        }
    }
    rt->DisableScissor();
    rt->UnBind(gfx_);

    auto tex = rt->GetColorAttachment(AttachmentPoint::kColor0);
    tex->ReadBackImage(minx, miny, sizex, sizey, 0, 0,
        [sizex, sizey, this](const uint8_t* data, size_t raw_pitch) {
            int pick_id = -1;
            int max_hit = 0;
            std::map<int, int> pick_item;

            for (int y = 0; y < sizey; ++y) {
                const uint8_t* texel = data;
                for (int x = 0; x < sizex; ++x) {
                    auto id = *((const int32_t*)texel);
                    texel += sizeof(uint32_t);

                    auto it = pick_item.find(id);
                    int hit = it == pick_item.end() ? 1 : it->second + 1;
                    pick_item[id] = hit;
                    if (hit > max_hit) {
                        pick_id = id;
                    }
                }
                data += raw_pitch;
            }

            selected_go_ = nullptr;
            if (pick_id > 0) {
                auto mr = RenderableManager::Instance()->Find(pick_id);
                if (mr) {
                    selected_go_ = mr->game_object();
                }
            }
    });
}

void Editor::DrawGizmos() {
    if (selected_go_) {
        selected_go_->DrawSelectedGizmos();
    }
}

void Editor::DrawPanel() {
    DrawScenePanel();
    DrawInspectorPanel();
}

void Editor::DrawInspectorPanel() {
    if (!selected_go_) return;

    ImGui::SetNextWindowPos(ImVec2(width_ * 0.775f, height_ * 0.05f), ImGuiCond_Once);// ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(width_ * 0.20f, height_ * 0.7f), ImGuiCond_Once);// ImGuiCond_FirstUseEver);

    ImGuiWindowFlags window_flags = 0;
    if (ImGui::Begin("Inspector", nullptr, window_flags)) {
        if (ImGui::RadioButton("Active", selected_go_->IsActive())) {
            if (selected_go_->IsActive()) {
                selected_go_->Deactivate();
            }
            else {
                selected_go_->Activate();
            }
        }
        selected_go_->DrawInspector();
    }

    ImGui::End();
}

void Editor::DrawScenePanel() {
    ImGui::SetNextWindowPos(ImVec2(width_ * 0.015f, height_ * 0.05f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(width_ * 0.2f, height_ * 0.7f), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags window_flags = 0;
    if (!ImGui::Begin("Scene Hierarchy", nullptr, window_flags)) {
        // Early out if the window is collapsed, as an optimization.
        ImGui::End();
        return;
    }

    uint32_t cur_selected = selected_go_ ? selected_go_->id() : 0;
    uint32_t selected = cur_selected;
    auto& list = GameObjectManager::Instance()->GetSceneNodeList();
    for (auto o : list) {
        o->DrawSceneNode(selected);
    }

    if (selected != cur_selected) {
        selected_go_ = GameObjectManager::Instance()->Find(selected);
    }

    ImGui::End();
}

void Editor::RegisterHighLightPass(GfxDriver* gfx, Renderer* renderer) {
    auto& render_graph = renderer->render_graph();
    auto outline_mat = std::make_shared<Material>("outline", TEXT("Solid"));
    outline_mat->SetProperty("object_transform", Renderable::GetTransformCBuffer(gfx));

    RasterStateDesc outline_rs;
    outline_rs.depthWrite = false;
    outline_rs.depthFunc = CompareFunc::kAlways;
    outline_rs.stencilEnable = true;
    outline_rs.stencilFunc = CompareFunc::kAlways;
    outline_rs.depthStencilPassOp = StencilOp::kReplace;
    outline_mat->GetTemplate()->SetRasterState(outline_rs);
    outline_mat->GetTemplate()->SetInputLayout(Mesh::kDefaultLayout);
    
    render_graph.AddPass("outline mask",
        [&](PassNode& pass) {
        },
        [this, renderer, outline_mat](Renderer* renderer, const PassNode& pass) {
            if (!selected_go_) return;
            auto mr = selected_go_->GetComponent<MeshRenderer>();
            if (!mr) return;

            renderer->render_target()->BindDepthStencil(gfx_);
            pass.Render(renderer, mr, outline_mat.get());
        });

    auto desc = Texture::Description()
        .SetDimension(renderer->render_target()->width() / 2, renderer->render_target()->height() / 2)
        .SetCreateFlag(CreateFlags::kRenderTarget)
        .SetFormat(TextureFormat::kR8G8B8A8_UNORM);

    auto outline_draw_tex = gfx->CreateTexture(desc);
    outline_draw_tex->SetName(TEXT("Outline draw texture"));

    auto outline_draw_rt = gfx->CreateRenderTarget(outline_draw_tex->width(), outline_draw_tex->height());
    outline_draw_rt->AttachColor(AttachmentPoint::kColor0, outline_draw_tex);

    auto solid_mat = MaterialManager::Instance()->Get("solid");
    auto outline_solid_mat = std::make_shared<Material>(*solid_mat);
    outline_solid_mat->SetProperty("object_transform", Renderable::GetTransformCBuffer(gfx));

    outline_solid_mat->SetProperty("color", Color{ 1.0f, 0.4f, 0.4f, 1.0f });

    render_graph.AddPass("outline draw",
        [&](PassNode& pass) {
        },
        [this, outline_draw_rt, outline_solid_mat](Renderer* renderer, const PassNode& pass) {
            if (!selected_go_) return;
            auto mr = selected_go_->GetComponent<MeshRenderer>();
            if (!mr) return;

            outline_draw_rt->Clear();
            RenderTargetBindingGuard gurad(gfx_, outline_draw_rt.get());

            pass.Render(renderer, mr, outline_solid_mat.get());
        });

    SetKernelGauss(blur_param_, 4, 2.0);
    auto blur_param = gfx->CreateConstantBuffer<BlurParam>(blur_param_, UsageType::kDefault);
    auto blur_dir = gfx->CreateConstantBuffer<BlurDirection>(blur_dir_);

    auto outline_desc = Texture::Description()
        .SetDimension(renderer->render_target()->width() / 2, renderer->render_target()->height() / 2)
        .SetCreateFlag(CreateFlags::kRenderTarget)
        .SetFormat(TextureFormat::kR8G8B8A8_UNORM);

    auto outline_htex = gfx->CreateTexture(outline_desc);
    outline_htex->SetName(TEXT("horizontal outline draw texture"));

    auto outline_hrt = gfx->CreateRenderTarget(outline_htex->width(), outline_htex->height());
    outline_hrt->AttachColor(AttachmentPoint::kColor0, outline_htex);

    SamplerState ss;
    ss.warpU = ss.warpV = WarpMode::kMirror;
    ss.filter = FilterMode::kPoint;

    RasterStateDesc hblur_rs;
    hblur_rs.depthEnable = false;
    hblur_rs.depthWrite = true;
    hblur_rs.depthFunc = RasterStateDesc::kDefaultDepthFunc;
    
    auto hblur_mat = std::make_shared<PostProcessMaterial>("hightlight", TEXT("BlurOutline"));

    hblur_mat->GetTemplate()->SetRasterState(hblur_rs);
    hblur_mat->SetProperty("Kernel", blur_param);
    hblur_mat->SetProperty("Control", blur_dir);
    hblur_mat->SetProperty("tex_sam", ss);
    hblur_mat->SetProperty("tex", outline_draw_tex);

    render_graph.AddPass("horizontal blur",
        [&](PassNode& pass) {
        },
        [this, outline_hrt, blur_dir, outline_draw_rt, hblur_mat](Renderer* renderer, const PassNode& pass) {
            if (!selected_go_) return;
            auto mr = selected_go_->GetComponent<MeshRenderer>();
            if (!mr) return;

            outline_hrt->ClearColor(AttachmentPoint::kColor0);
            blur_dir_.isHorizontal = true;
            blur_dir->Update(&blur_dir_);

            auto& mgr = renderer->GetPostProcessManager();
            mgr.Process(outline_hrt.get(), hblur_mat.get());
        });

    SamplerState vss;
    vss.warpU = vss.warpV = WarpMode::kMirror;
    vss.filter = FilterMode::kPoint;

    auto vblur_mat = std::make_shared<PostProcessMaterial>("hightlight", TEXT("BlurOutline"));
    vblur_mat->SetProperty("tex_sam", vss);

    RasterStateDesc blur_rs;
    blur_rs.depthWrite = false;
    blur_rs.stencilEnable = true;
    blur_rs.depthFunc = CompareFunc::kAlways;
    blur_rs.stencilFunc = CompareFunc::kNotEqual;
    blur_rs.depthStencilPassOp = StencilOp::kKeep;
    blur_rs.blendFunctionSrcRGB = BlendFunction::kSrcAlpha;
    blur_rs.blendFunctionDstRGB = BlendFunction::kOneMinusSrcAlpha;
    vblur_mat->GetTemplate()->SetRasterState(blur_rs);
    vblur_mat->SetProperty("Kernel", blur_param);
    vblur_mat->SetProperty("Control", blur_dir);
    vblur_mat->SetProperty("tex_sam", ss);
    vblur_mat->SetProperty("tex", outline_htex);

    render_graph.AddPass("vertical blur",
        [&](PassNode& pass) {
        },
        [this, blur_dir, outline_hrt, vblur_mat](Renderer* renderer, const PassNode& pass) {
            if (!selected_go_) return;
            auto mr = selected_go_->GetComponent<MeshRenderer>();
            if (!mr) return;

            blur_dir_.isHorizontal = false;
            blur_dir->Update(&blur_dir_);

            auto& mgr = renderer->GetPostProcessManager();
            mgr.Process(renderer->render_target().get(), vblur_mat.get());
        });
}

template<typename T>
static constexpr T gauss(T x, T sigma) noexcept {
    const auto ss = sigma * sigma;
    return ((T)1.0 / ::sqrt((T)2.0 * (T)math::kPI * ss)) * ::exp(-(x * x) / ((T)2.0 * ss));
}

void Editor::SetKernelGauss(BlurParam& param, int radius, float sigma) {
    const int nTaps = radius * 2 + 1;
    param.nTaps = nTaps;
    float sum = 0.0f;
    for (int i = 0; i < nTaps; i++)
    {
        const auto x = float(i - radius);
        const auto g = gauss(x, sigma);
        sum += g;
        param.coefficients[i] = g;
    }
    for (int i = 0; i < nTaps; i++)
    {
        param.coefficients[i] = (float)param.coefficients[i] / sum;
    }
}

}
}
