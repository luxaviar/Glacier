#pragma once

#include "Render/Graph/GraphResource.h"

namespace glacier {
namespace render {

class GraphResourceTable;
class PassNode;

// The declaration surface a pass is set up with (see
// .docs/render-graph-plan.md §4.3):
//
//     graph.AddPass("skybox",
//         [=](PassBuilder& b) {
//             b.Read(hdr_depth, ResourceAccessBit::kDepthWrite);
//             b.Write(hdr_color, ResourceAccessBit::kRenderTarget);
//         },
//         [=](PassContext& ctx, const PassNode& pass) { ... });
//
// What is declared here is what the graph orders the pass by and what it makes
// the resources hold while the pass runs; a use the body makes without
// declaring it is not an error yet - the validation of §4.6 counts it, which is
// how a pass that is still to be migrated shows up.
class PassBuilder {
public:
    PassBuilder(PassNode& pass, GraphResourceTable& resources) :
        pass_(pass),
        resources_(&resources)
    {}

    void Read(GraphResourceHandle resource, ResourceAccessBit access);
    void Write(GraphResourceHandle resource, ResourceAccessBit access);

    // The resource is written as the given attachment of the pass. The state
    // is implied by the attachment; the binding itself stays in the body (D2).
    void SetRenderTarget(GraphResourceHandle resource, AttachmentPoint point = AttachmentPoint::kColor0);
    void SetDepthStencil(GraphResourceHandle resource);

    // A texture the graph creates for the frame and takes back at the end of
    // it. Declare how it is used with Read / Write as well.
    GraphResourceHandle CreateTexture(const char* name, const GraphTextureDesc& desc);

    // The pass writes something the outside reads, so the cull must keep it.
    void SetSideEffect(bool enable = true);

private:
    PassNode& pass_;
    GraphResourceTable* resources_;
};

} // namespace render
} // namespace glacier
