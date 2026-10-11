#include "RenderGraph.h"
#include "PassContext.h"
#include "PassNode.h"
#include "PassObserver.h"
#include "Common/Log.h"
#include <format>

namespace glacier {
namespace render {

RenderGraphCompileException::RenderGraphCompileException(int line, const TCHAR* file, std::string desc) noexcept :
    BaseException(line, file, desc)
{}

const TCHAR* RenderGraphCompileException::type() const noexcept {
    return TEXT("Render Graph Compile Exception");
}

RenderGraph::RenderGraph()
{
}

RenderGraph::~RenderGraph()
{}

PassNode& RenderGraph::CreatePass(const char* name, std::unique_ptr<PassExecutor>&& executor) {
    for (auto& p : passes_) {
        if (p.name() == name) {
            throw RGC_EXCEPTION("In RenderGraph::CreatePass, pass name duplicated: " + std::string(name));
        }
    }

    passes_.emplace_back(name, std::move(executor));
    dirty_ = true;
    return passes_.back();
}

GraphResourceHandle RenderGraph::Import(const char* name, Texture* texture,
    ResourceAccessBit initial_state, ResourceAccessBit final_state, bool external) {
    dirty_ = true;
    return resources_.AddImported(name, texture, initial_state, final_state, external);
}

PassNode& RenderGraph::GetPass(const char* passName)
{
    for(auto& p : passes_)
    {
        if( p.name() == passName )
        {
            return p;
        }
    }
    throw RGC_EXCEPTION( "In RenderGraph::GetPass, pass not found: " + std::string(passName));
}

void RenderGraph::SetRenderSize(uint32_t width, uint32_t height) {
    if (width == render_width_ && height == render_height_) {
        return;
    }

    render_width_ = width;
    render_height_ = height;
    dirty_ = true;
}

void RenderGraph::cull(bool enable) {
    if (options_.cull == enable) {
        return;
    }

    options_.cull = enable;
    dirty_ = true;
}

void RenderGraph::Compile() {
    compile_desc_ = {};
    compile_desc_.options = options_;

    compile_desc_.resources.reserve(resources_.size());
    for (const auto& entry : resources_.entries()) {
        GraphResourceDesc desc;
        desc.name = entry.name;
        desc.imported = entry.kind == GraphResourceKind::kImported;
        desc.fixed_state = entry.fixed_state;
        desc.external = entry.external;
        desc.initial_state = entry.initial_state;
        desc.final_state = entry.final_state;
        compile_desc_.resources.push_back(std::move(desc));
    }

    compile_desc_.passes.reserve(passes_.size());
    for (const auto& pass : passes_) {
        GraphPassDesc desc;
        desc.name = pass.name();
        desc.side_effect = pass.side_effect();
        desc.usages.reserve(pass.usages().size());

        for (const auto& usage : pass.usages()) {
            desc.usages.push_back({ usage.resource.index, usage.access });
        }

        compile_desc_.passes.push_back(std::move(desc));
    }

    compiled_ = CompileRenderGraph(compile_desc_);
    if (!compiled_.ok) {
        throw RGC_EXCEPTION("In RenderGraph::Compile, " + compiled_.error);
    }

    baked_ = true;
    dirty_ = false;

#if IS_DEBUG
    LOG_DEBUG("render graph compiled:\n{}", Dump());
#endif
}

Texture* RenderGraph::ResolveTexture(GraphResourceHandle handle) const {
    if (!resources_.Contains(handle)) {
        return nullptr;
    }

    return ResolveTextureEntry(resources_.entry(handle));
}

Texture* RenderGraph::ResolveTextureEntry(const GraphResourceEntry& entry) const {
    return entry.kind == GraphResourceKind::kImported ? entry.texture : entry.owned.get();
}

void RenderGraph::EnsureTransientAllocated(CommandBuffer* cmd_buffer, uint32_t resource) {
    auto& entry = resources_.entries()[resource];
    if (entry.kind != GraphResourceKind::kTransient || entry.owned) {
        return;
    }

    auto desc = entry.texture_desc.Resolve(render_width_, render_height_);
    entry.owned = allocator_.Acquire(desc);
    entry.owned->SetName(entry.name.c_str());
    cmd_buffer->AddGraphResource(entry.owned.get());
}

void RenderGraph::Execute(CommandBuffer* cmd_buffer, PassObserver* observer) {
    assert(baked_);
    if (dirty_) {
        Compile();
    }

    allocator_.BeginFrame();

#if IS_DEBUG
    // Probe of the render graph refactor (see .docs/render-graph-plan.md, T0.3):
    // the order the passes run in is what the dependency-ordered version is
    // compared against, and this line is that record. Only the first frames are
    // logged - the order is compiled once, so it is the same for every frame of
    // a run.
    constexpr uint32_t kProbeFrames = 3;
    static uint32_t probe_frames = 0;
    if (probe_frames < kProbeFrames) {
        ++probe_frames;

        std::string sequence;
        for (const auto& compiled_pass : compiled_.passes) {
            const auto& p = passes_[compiled_pass.pass];
            if (!p.active()) {
                continue;
            }

            if (!sequence.empty()) {
                sequence += " -> ";
            }
            sequence += p.name();
        }

        LOG_DEBUG("render graph pass sequence: {}", sequence);
    }
#endif

    cmd_buffer->BeginGraphScope();

    for (const auto& entry : resources_.entries()) {
        auto* texture = ResolveTextureEntry(entry);
        if (texture) {
            cmd_buffer->AddGraphResource(texture);
        }
    }

    GraphStats stats;
    stats.culled = (uint32_t)compiled_.culled.size();

    for (const auto& compiled_pass : compiled_.passes) {
        auto& pass = passes_[compiled_pass.pass];
        if (!pass.active()) {
            continue;
        }

        for (const auto& usage : compiled_pass.expected) {
            EnsureTransientAllocated(cmd_buffer, usage.resource);
        }

        // The group of transitions the graph takes for one pass is submitted on
        // its own: a resource moved twice inside one barrier call is rebuilt by
        // the driver and a debug layer complains, and what the pass body left
        // behind belongs to the pass, not to this group.
        cmd_buffer->FlushBarriers();

        // Move the resources to the states this pass declared, then hold those
        // states as what the pass expects: a transition the pass body sends to
        // the state it declared is the free one, anything else takes the old
        // immediate path and is counted (see .docs/render-graph-plan.md §4.6).
        // The transitions are sent even where the plan expected the resource to
        // be there already - the plan's idea of the state can be older than
        // what a pass body left behind, and the tracker is the one that knows
        // (it skips a transition to the state a resource is already in).
        for (const auto& usage : compiled_pass.expected) {
            auto& entry = resources_.entries()[usage.resource];
            if (entry.fixed_state) {
                continue;
            }

            auto* texture = ResolveTextureEntry(entry);
            if (texture) {
                cmd_buffer->GraphTransitionBarrier(texture, usage.access);
                ++stats.barriers;
            }
        }

        cmd_buffer->FlushBarriers();

        cmd_buffer->BeginGraphPass(pass.name().c_str());
        for (const auto& usage : compiled_pass.expected) {
            auto* texture = ResolveTextureEntry(resources_.entries()[usage.resource]);
            if (texture) {
                cmd_buffer->SetGraphExpectedState(texture, usage.access);
            }
        }

        if (observer != nullptr) {
            observer->BeginPass(pass.name().c_str(), cmd_buffer);
        }

        PassContext ctx(cmd_buffer, *this, pass);
        pass.Execute(ctx);

        if (observer != nullptr) {
            observer->EndPass(cmd_buffer);
        }

        cmd_buffer->EndGraphPass();
        ++stats.passes;
    }

    // The frame ends where it declared it would: what the graph borrowed is
    // handed back in the state the renderer expects to find it in next time.
    cmd_buffer->FlushBarriers();

    for (const auto& entry : resources_.entries()) {
        auto* texture = ResolveTextureEntry(entry);
        if (!texture || entry.fixed_state) {
            continue;
        }

        cmd_buffer->GraphTransitionBarrier(texture, entry.final_state);
        ++stats.barriers;
    }

    cmd_buffer->FlushBarriers();

    cmd_buffer->EndGraphScope();

    stats.fallback_transitions = cmd_buffer->graph_fallback_count();
    stats_ = stats;
}

void RenderGraph::Reset() noexcept
{
    assert(baked_);

    for (auto& entry : resources_.entries()) {
        if (entry.kind == GraphResourceKind::kTransient && entry.owned) {
            allocator_.Release(std::move(entry.owned));
        }
    }

    for (auto& p : passes_)
    {
        p.Reset();
    }
}

std::string RenderGraph::Dump() const {
    struct AccessName {
        ResourceAccessBit bit;
        const char* name;
    };

    constexpr AccessName kAccessNames[] = {
        { ResourceAccessBit::kVertexAndConstantBuffer, "cbuffer" },
        { ResourceAccessBit::kIndexBuffer, "index" },
        { ResourceAccessBit::kRenderTarget, "rt" },
        { ResourceAccessBit::kShaderWrite, "uav" },
        { ResourceAccessBit::kDepthWrite, "depth" },
        { ResourceAccessBit::kDepthRead, "depth_read" },
        { ResourceAccessBit::kNonPixelShaderRead, "cs_read" },
        { ResourceAccessBit::kPixelShaderRead, "ps_read" },
        { ResourceAccessBit::kCopyDest, "copy_dst" },
        { ResourceAccessBit::kCopySource, "copy_src" },
    };

    auto access_name = [&](ResourceAccessBit access) {
        const uint64_t bits = (uint64_t)access;
        std::string name;
        for (const auto& entry : kAccessNames) {
            if ((bits & (uint64_t)entry.bit) != 0) {
                if (!name.empty()) {
                    name += "|";
                }
                name += entry.name;
            }
        }

        if (name.empty()) {
            name = std::format("0x{:x}", bits);
        }

        return name;
    };

    std::string out;
    out += std::format("render graph: {} passes, {} resources, cull {}\n",
        compiled_.passes.size(), resources_.size(), options_.cull ? "on" : "off");

    out += "resources:\n";
    for (size_t r = 0; r < resources_.size(); ++r) {
        const auto& entry = resources_.entries()[r];
        out += std::format("  [{}] {:<28} {:<9} {} -> {}{}{}\n", r, entry.name,
            entry.kind == GraphResourceKind::kImported ? "imported" : "transient",
            access_name(entry.initial_state), access_name(entry.final_state),
            entry.external ? " external" : "",
            entry.fixed_state ? " fixed" : "",
            entry.kind == GraphResourceKind::kTransient
                ? std::format(" scale {}", (int)entry.texture_desc.scale)
                : "");
    }

    out += std::format("passes (execution order, {} run):\n", compiled_.passes.size());
    for (size_t oi = 0; oi < compiled_.passes.size(); ++oi) {
        const auto& compiled_pass = compiled_.passes[oi];
        const auto& pass = passes_[compiled_pass.pass];
        out += std::format("  [{}] {}{}\n", oi, pass.name(), pass.active() ? "" : " (inactive)");

        for (const auto& usage : compiled_pass.expected) {
            const auto& entry = resources_.entries()[usage.resource];
            out += std::format("        {} [{}] {}\n",
                access_name(usage.access), usage.resource, entry.name);
        }

        for (const auto& barrier : compiled_pass.barriers) {
            out += std::format("        barrier [{}] {} -> {}\n", barrier.resource,
                access_name(barrier.before), access_name(barrier.after));
        }
    }

    if (compiled_.culled.empty()) {
        out += "culled: none\n";
    }
    else {
        out += "culled:";
        for (auto p : compiled_.culled) {
            out += " " + compile_desc_.passes[p].name;
        }
        out += "\n";
    }

    if (!compiled_.transients.empty()) {
        out += "transients:\n";
        for (const auto& transient : compiled_.transients) {
            out += std::format("  [{}] {} passes {}..{}\n", transient.resource,
                compile_desc_.resources[transient.resource].name,
                transient.first_pass, transient.last_pass);
        }
    }

    return out;
}

void RenderGraph::PrintStats() const {
    LOG_LOG("--- render graph: {} passes ran, {} culled, {} barriers requested, {} fallback transitions ---",
        stats_.passes, stats_.culled, stats_.barriers, stats_.fallback_transitions);
}

} // namespace render
} // namespace glacier
