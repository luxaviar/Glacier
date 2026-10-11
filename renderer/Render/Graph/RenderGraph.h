#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include "Exception/Exception.h"
#include "GraphResource.h"
#include "GraphResourceAllocator.h"
#include "PassBuilder.h"
#include "PassExecutor.h"
#include "RenderGraphCompiler.h"

namespace glacier {
namespace render {

class CommandBuffer;
class PassNode;
class PassObserver;

class RenderGraphCompileException : public BaseException {
public:
    RenderGraphCompileException(int line, const TCHAR* file, std::string desc) noexcept;
    const TCHAR* type() const noexcept override;
};

#define RGC_EXCEPTION(message) RenderGraphCompileException(__LINE__, TEXT(__FILE__), (message))

// What the newest Execute did (see .docs/render-graph-plan.md §4.5); F4 prints
// it next to the pass timings.
struct GraphStats {
    uint32_t passes = 0;
    uint32_t culled = 0;
    uint32_t barriers = 0;
    uint32_t fallback_transitions = 0;
};

// The frame graph (see .docs/render-graph-plan.md §4): the renderer registers
// its passes and the resources they use once, the graph compiles what was
// declared - execution order, culled passes, barrier plan, transient plan - and
// every frame it runs the result. Pass names stay what they always were, since
// they are the material pass keys (D7), and what a pass body sees changes only
// in that it runs with a PassContext instead of the raw command buffer.
class RenderGraph {
public:
    RenderGraph();
    ~RenderGraph();

    RenderGraph(const RenderGraph&) = delete;

    // —— resources, registered once ——
    // An imported resource is owned outside the graph: the graph borrows it and
    // manages only its state. It starts the frame in initial_state and the
    // graph leaves it in final_state; external marks content the outside of the
    // graph consumes, which is what the cull keeps a pass alive for.
    GraphResourceHandle Import(const char* name, Texture* texture,
        ResourceAccessBit initial_state, ResourceAccessBit final_state,
        bool external = false);

    // —— passes, registered once ——
    // setup is called with the PassBuilder to declare what the pass reads and
    // writes; execute is called every frame with the PassContext. Both may take
    // the pass's Data when the Data template argument is given.
    template<typename Data = nullptr_t, typename Setup, typename Execute>
    PassNode& AddPass(const char* name, Setup setup, Execute&& execute) {
        auto executor = std::make_unique<RenderPassExecutor<Data, Execute>>(std::forward<Execute>(execute));
        auto& data = executor->data();
        auto& pass = CreatePass(name, std::move(executor));

        PassBuilder builder(pass, resources_);
        if constexpr (std::is_same<Data, nullptr_t>::value) {
            setup(builder);
        }
        else {
            setup(data, builder);
        }

        return pass;
    }

    PassNode& GetPass(const char* pass_name);

    // —— compile and run ——
    // Compiles what is registered (§4.4). Runs by itself before Execute when the
    // render size or the options changed since the last one (D6).
    void Compile();
    // Runs the compiled frame: observer, when there is one, is told where every
    // pass starts and ends; it is how the GPU pass timer of PerfStats brackets
    // the passes of the graph.
    void Execute(CommandBuffer* cmd_buffer, PassObserver* observer = nullptr);
    // Releases what the frame took - the transients - and ends the allocator
    // frame; called after the frame was presented (§4.5).
    void Reset() noexcept;

    // The size transient textures are resolved against; a change recompiles
    void SetRenderSize(uint32_t width, uint32_t height);

    // Drops the passes no live root reaches (§4.4 item 2). Off until the
    // migration is done, so the order matches the registration order.
    void cull(bool enable);
    bool cull() const { return options_.cull; }

    // The texture a handle names; null before a transient was created for the
    // frame (PassContext resolves handles through this)
    Texture* ResolveTexture(GraphResourceHandle handle) const;
    // The shared pointer of a transient of the frame; null for an imported
    // resource, whose lifetime is somebody else's
    std::shared_ptr<Texture> ResolveTextureShared(GraphResourceHandle handle) const;

    const GraphStats& stats() const { return stats_; }

    // What the graph will run and why: order, per pass reads/writes and the
    // barrier plan, culled passes, transients (§4.4 item 6). Debug logs it once
    // per compile.
    std::string Dump() const;
    // The one line F4 writes next to the pass timings of PerfStats
    void PrintStats() const;

protected:
    PassNode& CreatePass(const char* name, std::unique_ptr<PassExecutor>&& executor);

    Texture* ResolveTextureEntry(const GraphResourceEntry& entry) const;
    void EnsureTransientAllocated(CommandBuffer* cmd_buffer, uint32_t resource);

    bool baked_ = false;
    bool dirty_ = false;
    GraphCompileOptions options_;

    uint32_t render_width_ = 0;
    uint32_t render_height_ = 0;

    // The passes live behind stable addresses: what AddPass hands back (a
    // PassNode&) has to stay valid across the registrations that come after it
    // - the runtime toggles of a frame hold on to the pass they switch (T2.2).
    std::vector<std::unique_ptr<PassNode>> passes_;
    GraphResourceTable resources_;
    GraphResourceAllocator allocator_;

    GraphCompileDesc compile_desc_;
    CompiledGraph compiled_;
    GraphStats stats_;
};

} // namespace render
} // namespace glacier
