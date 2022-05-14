#pragma once

#include "RenderTexturePool.h"
#include "GfxDriver.h"

namespace glacier {
namespace render {

std::vector<std::shared_ptr<Texture>> RenderTexturePool::free_textures_;

std::shared_ptr<Texture> RenderTexturePool::Get(uint32_t width, uint32_t height,
    TextureFormat format, CreateFlags flags, uint32_t sample_count ,uint32_t sample_quality)
{
    auto it = std::find_if(free_textures_.begin(), free_textures_.end(),
        [=](std::shared_ptr<Texture>& tex) {
            if (tex->GetFormat() == format && tex->width() == width, tex->height() == height && tex->GetFlags() == (uint32_t)flags) {
                return true;
            }

            return false;
        });

    if (it != free_textures_.end()) {
        auto ret = *it;
        free_textures_.erase(it);
        return ret;
    }

    auto desc = Texture::Description()
        .SetDimension(width, height)
        .SetFormat(format)
        .SetCreateFlag(flags)
        .SetSampleDesc(sample_count, sample_quality);

    auto tex = GfxDriver::Get()->CreateTexture(desc);
    return tex;
}

void RenderTexturePool::Release(std::shared_ptr<Texture>& texture) {
    free_textures_.push_back(texture);
}

}
}
