#include "picking.h"
#include <algorithm>
#include <map>
#include "render/camera.h"
#include "core/objectmanager.h"
#include "render/base/renderable.h"
#include "render/base/ConstantBuffer.h"
#include "render/base/rendertarget.h"
#include "render/image.h"

namespace glacier {
namespace render {

Picking::Picking(GfxDriver* gfx) : gfx_(gfx) {
    rs_.scissor = true;
}

int Picking::Detect(int x, int y, Camera* camera, const std::vector<Renderable*>& visibles, std::shared_ptr<RenderTarget>& rt) {
    if (visibles.empty()) return -1;

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

    int pick_id = -1;
    if (sizex > 0 && sizey > 0) {
        ScissorRect rect{ minx, miny, maxx, maxy };
        rt->EnableScissor(rect);

        Vec4f encoded_id;
        if (!color_buf_) {
            color_buf_ = gfx_->CreateConstantBuffer<Vec4f>(encoded_id);//  std::make_unique<ConstantBuffer<Vec4f>>(gfx, encoded_id);
        }

        rt->Clear({ 0, 0, 0, 0 });
        rt->Bind();
        gfx_->UpdatePipelineState(rs_);
        gfx_->BindCamera(camera);

        auto mat = MaterialManager::Instance()->Get("solid");
        {
            MaterialGuard guard(gfx_, mat);
            for (auto o :  visibles) {
                if (!o->IsActive() || !o->IsPickable()) continue;

                auto id = o->id();
                //a b g r uint32_t
                encoded_id.r = (id & 0xFF) / 255.0f;
                encoded_id.g = ((id >> 8) & 0xFF) / 255.0f;
                encoded_id.b = ((id >> 16) & 0xFF) / 255.0f;
                encoded_id.a = ((id >> 24) & 0xFF) / 255.0f;
                color_buf_->Update(&encoded_id);
                color_buf_->Bind(ShaderType::kPixel, 0);

                o->Render(gfx_);
            }
        }
        //pass.PostRender(gfx);
        rt->DisableScissor();

        Image tmp_img(sizex, sizey);
        auto tex = rt->GetColorAttachment(AttachmentPoint::kColor0);
        tex->ReadBackImage(tmp_img, minx, miny, sizex, sizey, 0, 0);

        int max_hit = 0;
        std::map<int, int> pick_item;
        const uint32_t* pixel = (const uint32_t*)tmp_img.GetRawPtr<ColorRGBA32>();
        for (int i = 0; i < sizex * sizey; ++i) {
            uint32_t col = pixel[i];
            int id = col;
            auto it = pick_item.find(id);
            int hit = it == pick_item.end() ? 1 : it->second + 1;
            pick_item[id] = hit;
            if (hit > max_hit) {
                pick_id = id;
            }
        }
    }

    return pick_id;
}

}
}
