#include "PassContext.h"
#include "RenderGraph.h"

namespace glacier {
namespace render {

Texture* PassContext::GetTexture(GraphResourceHandle handle) const {
    return graph_->ResolveTexture(handle);
}

} // namespace render
} // namespace glacier
