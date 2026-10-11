#include "PassBuilder.h"
#include <assert.h>
#include "GraphResource.h"
#include "PassNode.h"

namespace glacier {
namespace render {

void PassBuilder::Read(GraphResourceHandle resource, ResourceAccessBit access) {
    assert(resources_->Contains(resource));
    pass_.AddUsage(resource, access);
}

void PassBuilder::Write(GraphResourceHandle resource, ResourceAccessBit access) {
    assert(resources_->Contains(resource));
    pass_.AddUsage(resource, access);
}

void PassBuilder::SetRenderTarget(GraphResourceHandle resource, AttachmentPoint point) {
    assert(resources_->Contains(resource));
    pass_.AddUsage(resource, ResourceAccessBit::kRenderTarget);
    pass_.SetRenderTarget(resource, point);
}

void PassBuilder::SetDepthStencil(GraphResourceHandle resource) {
    assert(resources_->Contains(resource));
    pass_.AddUsage(resource, ResourceAccessBit::kDepthWrite);
    pass_.SetDepthStencil(resource);
}

GraphResourceHandle PassBuilder::CreateTexture(const char* name, const GraphTextureDesc& desc) {
    return resources_->AddTransient(name, desc);
}

void PassBuilder::SetSideEffect(bool enable) {
    pass_.SetSideEffect(enable);
}

} // namespace render
} // namespace glacier
