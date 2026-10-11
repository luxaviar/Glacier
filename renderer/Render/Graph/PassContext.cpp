#include "PassContext.h"
#include "RenderGraph.h"

namespace glacier {
namespace render {

Texture* PassContext::GetTexture(GraphResourceHandle handle) const {
    return graph_->ResolveTexture(handle);
}

std::shared_ptr<Texture> PassContext::GetTextureShared(GraphResourceHandle handle) const {
    return graph_->ResolveTextureShared(handle);
}

} // namespace render
} // namespace glacier
