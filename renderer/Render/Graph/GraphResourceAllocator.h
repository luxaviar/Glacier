#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include "Render/Base/Texture.h"

namespace glacier {
namespace render {

// The transient side of the resource table: textures the graph creates for a
// frame and takes back at the end of it, built on RenderTexturePool so the
// storage is reused instead of grown. A texture the frame released can only go
// back to the pool once the GPU is done with the frames still in flight, so the
// release is queued and the pool gets it kRetireDelay frames later - no new
// synchronization is needed, the existing frame fence is that far ahead.
class GraphResourceAllocator {
public:
    // GfxDriver::kBufferCount: a resource released by frame F is taken back at
    // the start of frame F + this, by which time F has completed.
    static constexpr uint32_t kRetireDelay = 3;

    explicit GraphResourceAllocator(uint32_t retire_delay = kRetireDelay) :
        retire_delay_(retire_delay)
    {}

    // Once per frame, before the graph runs: hands back what is due.
    void BeginFrame();

    std::shared_ptr<Texture> Acquire(const TextureDescription& desc);
    // Queues the texture; it reaches the pool retire_delay_ frames from now.
    void Release(std::shared_ptr<Texture> texture);

    uint32_t pending() const { return (uint32_t)retiring_.size(); }

private:
    struct Retired {
        std::shared_ptr<Texture> texture;
        uint64_t frame = 0;
    };

    uint32_t retire_delay_;
    uint64_t frame_ = 0;
    std::vector<Retired> retiring_;
};

} // namespace render
} // namespace glacier
