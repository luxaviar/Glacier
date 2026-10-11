#include "GraphResourceAllocator.h"
#include <algorithm>
#include "Render/Base/RenderTexturePool.h"

namespace glacier {
namespace render {

void GraphResourceAllocator::BeginFrame() {
    ++frame_;

    auto it = std::remove_if(retiring_.begin(), retiring_.end(), [this](Retired& retired) {
        if (frame_ - retired.frame < retire_delay_) {
            return false;
        }

        RenderTexturePool::Release(retired.texture);
        return true;
    });

    retiring_.erase(it, retiring_.end());
}

std::shared_ptr<Texture> GraphResourceAllocator::Acquire(const TextureDescription& desc) {
    return RenderTexturePool::Get(desc.width, desc.height, desc.format,
        (CreateFlags)desc.create_flags, desc.sample_count, desc.sample_quality);
}

void GraphResourceAllocator::Release(std::shared_ptr<Texture> texture) {
    if (!texture) {
        return;
    }

    retiring_.push_back({ std::move(texture), frame_ });
}

} // namespace render
} // namespace glacier
