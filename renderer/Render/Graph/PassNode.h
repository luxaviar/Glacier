#pragma once

#include <string>
#include <vector>
#include <array>
#include <memory>
#include "PassExecutor.h"
#include "GraphResource.h"
#include "PassContext.h"
#include "Render/Base/Renderable.h"
#include "Core/Identifiable.h"

namespace glacier {
namespace render {

class Material;
class CommandBuffer;
class Renderer;

class PassNode : public Identifiable<PassNode> {
public:
    // One declared use of a resource; the access bit is the state the pass
    // wants the resource in while it runs.
    struct Usage {
        GraphResourceHandle resource;
        ResourceAccessBit access = ResourceAccessBit::kCommon;
    };

    // One attachment the pass declared it writes.
    struct Target {
        GraphResourceHandle resource;
        AttachmentPoint point = AttachmentPoint::kColor0;
    };

    PassNode(const char* name) noexcept;
    PassNode(const char* name, std::unique_ptr<PassExecutor>&& executor) noexcept;

    PassNode(const PassNode& other) = delete;
    PassNode(PassNode&& other) noexcept = default;
    virtual ~PassNode() = default;

    const std::string& name() const { return name_; }

    virtual void Setup(Renderer* renderer);
    virtual void Execute(PassContext& ctx);

    void Render(CommandBuffer* cmd_buffer, const std::vector<Renderable*>& objs, Material* mat = nullptr) const;
    void Render(CommandBuffer* cmd_buffer, const Renderable* obj = nullptr, Material* mat = nullptr) const;

    //Draws a list of objects that all use `mat`, batching the ones that share a
    //mesh and a material into one instanced draw (see Renderable::CanBatchWith).
    static void RenderList(CommandBuffer* cmd_buffer, const std::vector<Renderable*>& objs, Material* mat);

    void Reset();
    void Finalize() const;

    bool active() const { return active_; }
    void active(bool enable) { active_ = enable; }

    // What the pass declared, for the compiler (see RenderGraphCompiler.h)
    const std::vector<Usage>& usages() const { return usages_; }
    const std::vector<Target>& render_targets() const { return render_targets_; }
    const GraphResourceHandle& depth_stencil() const { return depth_stencil_; }
    bool side_effect() const { return side_effect_; }

    // Called by PassBuilder while the pass is being set up
    void AddUsage(GraphResourceHandle resource, ResourceAccessBit access);
    void SetRenderTarget(GraphResourceHandle resource, AttachmentPoint point);
    void SetDepthStencil(GraphResourceHandle resource);
    void SetSideEffect(bool enable) { side_effect_ = enable; }

protected:
    bool active_ = true;
    bool side_effect_ = false;
    std::string name_;

    std::vector<Usage> usages_;
    std::vector<Target> render_targets_;
    GraphResourceHandle depth_stencil_;

    std::unique_ptr<PassExecutor> executor_;
};

}
}
