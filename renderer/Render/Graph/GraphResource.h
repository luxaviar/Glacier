#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "Render/Base/Enums.h"
#include "Render/Base/Texture.h"

namespace glacier {
namespace render {

// A handle into the resource table of the render graph (see
// .docs/render-graph-plan.md §4.2). The table is append-only, so a handle stays
// valid for the life of the graph, and it carries the version of the slot it
// names, so a handle that outlives its slot can be told apart instead of
// resolving into whatever took its place.
struct GraphResourceHandle {
    static constexpr uint32_t kInvalidIndex = 0xFFFFFFFF;

    uint32_t index = kInvalidIndex;
    uint32_t version = 0;

    bool valid() const { return index != kInvalidIndex; }

    bool operator==(const GraphResourceHandle& rhs) const {
        return index == rhs.index && version == rhs.version;
    }

    bool operator!=(const GraphResourceHandle& rhs) const {
        return !(*this == rhs);
    }
};

// How a transient texture follows the render size: absolutely (the size it was
// declared with) or as a fraction of it.
enum class TextureScale : uint8_t {
    kAbsolute,
    kFull,
    kHalf,
    kQuarter,
};

// The description of a transient texture: a TextureDescription plus the size
// it gets at the render size of the frame.
struct GraphTextureDesc {
    static GraphTextureDesc Scale(TextureScale scale);

    GraphTextureDesc& Format(TextureFormat format);
    GraphTextureDesc& CreateFlag(CreateFlags flag);
    GraphTextureDesc& SampleDesc(uint32_t count, uint32_t quality = 0);
    GraphTextureDesc& Dimension(uint32_t width, uint32_t height);

    // The concrete description this asks for at a render size.
    TextureDescription Resolve(uint32_t render_width, uint32_t render_height) const;

    TextureDescription desc;
    TextureScale scale = TextureScale::kAbsolute;
};

enum class GraphResourceKind : uint8_t {
    kImported,
    kTransient,
};

// One slot of the table.
struct GraphResourceEntry {
    GraphResourceHandle handle;
    std::string name;
    GraphResourceKind kind = GraphResourceKind::kImported;

    GraphTextureDesc texture_desc;   // transient: what to create at the render size
    Texture* texture = nullptr;      // imported: owned outside the graph
    std::shared_ptr<Texture> owned;  // transient: held until the frame that used it ends

    ResourceAccessBit initial_state = ResourceAccessBit::kCommon;
    ResourceAccessBit final_state = ResourceAccessBit::kPixelShaderRead;
    bool external = false;           // its content is consumed outside the graph
    bool fixed_state = false;        // the runtime rejects transitions on it
};

// Imported resources are the ones the renderer owns and the graph only borrows
// (hdr, ldr, gbuffer, the shadow map, ...): the graph manages their state, not
// their lifetime, and it is told which state they start and end the frame in.
// Transients are the ones the graph creates for whoever asks for one and takes
// back at the end of the frame. The table holds both; it resolves pointers and
// nothing else, so it runs without a GPU (see .docs/render-graph-plan.md §4.2).
class GraphResourceTable {
public:
    GraphResourceHandle AddImported(const char* name, Texture* texture,
        ResourceAccessBit initial_state, ResourceAccessBit final_state, bool external = false);
    GraphResourceHandle AddTransient(const char* name, const GraphTextureDesc& desc);

    size_t size() const { return entries_.size(); }
    bool empty() const { return entries_.empty(); }

    bool Contains(GraphResourceHandle handle) const;
    const GraphResourceEntry& entry(GraphResourceHandle handle) const;
    GraphResourceEntry& entry(GraphResourceHandle handle);

    const std::vector<GraphResourceEntry>& entries() const { return entries_; }
    std::vector<GraphResourceEntry>& entries() { return entries_; }

    // The texture a handle names, null before a transient was created for the
    // frame; never null for an imported one.
    Texture* Resolve(GraphResourceHandle handle) const;

    // The frame's transient behind a handle; an imported resource is owned
    // outside the graph, so this is null for it. A material that samples a
    // transient holds the texture as a shared pointer, so this is what
    // re-points such a material at the texture of the frame (T2.4).
    std::shared_ptr<Texture> ResolveShared(GraphResourceHandle handle) const;

private:
    std::vector<GraphResourceEntry> entries_;
};

} // namespace render
} // namespace glacier
