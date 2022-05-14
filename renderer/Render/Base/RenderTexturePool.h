#pragma once

#include "Texture.h"
#include "Common/Singleton.h"

namespace glacier {
namespace render {

class RenderTexturePool {
public:
    static std::shared_ptr<Texture> Get(uint32_t width, uint32_t height, TextureFormat format = TextureFormat::kR8G8B8A8_UNORM,
        CreateFlags flags = CreateFlags::kRenderTarget, uint32_t sample_count = 1, uint32_t sample_quality = 0);
    static void Release(std::shared_ptr<Texture>& texture);

private:
    static std::vector<std::shared_ptr<Texture>> free_textures_;
};

}
}
