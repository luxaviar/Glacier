#pragma once

#include "Render/Graph/GraphResource.h"

namespace glacier {
namespace render {

class CommandBuffer;
class PassNode;
class RenderGraph;

// What the body of a pass runs with (see .docs/render-graph-plan.md §4.3). It
// names the command buffer the pass records into and resolves the handles the
// pass declared back to what they hold; binding itself stays with the pass for
// now (D2), which keeps the diff of a migrated pass inside its registration.
class PassContext {
public:
    PassContext(CommandBuffer* cmd_buffer, const RenderGraph& graph, const PassNode& pass) :
        cmd_buffer_(cmd_buffer),
        graph_(&graph),
        pass_(&pass)
    {}

    CommandBuffer* cmd_buffer() const { return cmd_buffer_; }
    const PassNode& pass() const { return *pass_; }

    // The texture a handle names; null before a transient was created for the
    // frame (they are created at their first use, see RenderGraph::Execute).
    Texture* GetTexture(GraphResourceHandle handle) const;

    // The frame's transient behind a handle, for what holds a texture for
    // longer than one bind - a material property, which stores it as a shared
    // pointer. Null for an imported resource and before the transient of the
    // frame was created.
    std::shared_ptr<Texture> GetTextureShared(GraphResourceHandle handle) const;

private:
    CommandBuffer* cmd_buffer_;
    const RenderGraph* graph_;
    const PassNode* pass_;
};

} // namespace render
} // namespace glacier
