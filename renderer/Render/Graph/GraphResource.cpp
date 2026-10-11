#include "GraphResource.h"
#include <algorithm>
#include <assert.h>

namespace glacier {
namespace render {

GraphTextureDesc GraphTextureDesc::Scale(TextureScale scale) {
    GraphTextureDesc desc;
    desc.scale = scale;
    return desc;
}

GraphTextureDesc& GraphTextureDesc::Format(TextureFormat format) {
    desc.SetFormat(format);
    return *this;
}

GraphTextureDesc& GraphTextureDesc::CreateFlag(CreateFlags flag) {
    desc.SetCreateFlag(flag);
    return *this;
}

GraphTextureDesc& GraphTextureDesc::SampleDesc(uint32_t count, uint32_t quality) {
    desc.SetSampleDesc(count, quality);
    return *this;
}

GraphTextureDesc& GraphTextureDesc::Dimension(uint32_t width, uint32_t height) {
    desc.SetDimension(width, height);
    return *this;
}

TextureDescription GraphTextureDesc::Resolve(uint32_t render_width, uint32_t render_height) const {
    auto resolved = desc;

    switch (scale) {
    case TextureScale::kFull:
        resolved.SetDimension(render_width, render_height);
        break;
    case TextureScale::kHalf:
        resolved.SetDimension(std::max(1u, render_width / 2), std::max(1u, render_height / 2));
        break;
    case TextureScale::kQuarter:
        resolved.SetDimension(std::max(1u, render_width / 4), std::max(1u, render_height / 4));
        break;
    default:
        break;
    }

    return resolved;
}

GraphResourceHandle GraphResourceTable::AddImported(const char* name, Texture* texture,
    ResourceAccessBit initial_state, ResourceAccessBit final_state, bool external) {
    assert(texture);

    GraphResourceEntry entry;
    entry.handle = { (uint32_t)entries_.size(), 0 };
    entry.name = name;
    entry.kind = GraphResourceKind::kImported;
    entry.texture = texture;
    entry.initial_state = initial_state;
    entry.final_state = final_state;
    entry.external = external;
    entry.fixed_state = texture->has_fixed_state();

    entries_.push_back(std::move(entry));
    return entries_.back().handle;
}

GraphResourceHandle GraphResourceTable::AddTransient(const char* name, const GraphTextureDesc& desc) {
    GraphResourceEntry entry;
    entry.handle = { (uint32_t)entries_.size(), 0 };
    entry.name = name;
    entry.kind = GraphResourceKind::kTransient;
    entry.texture_desc = desc;
    entry.initial_state = ResourceAccessBit::kPixelShaderRead;
    entry.final_state = ResourceAccessBit::kPixelShaderRead;

    entries_.push_back(std::move(entry));
    return entries_.back().handle;
}

bool GraphResourceTable::Contains(GraphResourceHandle handle) const {
    return handle.valid() && handle.index < entries_.size() &&
        entries_[handle.index].handle.version == handle.version;
}

const GraphResourceEntry& GraphResourceTable::entry(GraphResourceHandle handle) const {
    assert(Contains(handle));
    return entries_[handle.index];
}

GraphResourceEntry& GraphResourceTable::entry(GraphResourceHandle handle) {
    assert(Contains(handle));
    return entries_[handle.index];
}

Texture* GraphResourceTable::Resolve(GraphResourceHandle handle) const {
    const auto& e = entry(handle);
    return e.kind == GraphResourceKind::kImported ? e.texture : e.owned.get();
}

std::shared_ptr<Texture> GraphResourceTable::ResolveShared(GraphResourceHandle handle) const {
    const auto& e = entry(handle);
    return e.kind == GraphResourceKind::kTransient ? e.owned : nullptr;
}

} // namespace render
} // namespace glacier
