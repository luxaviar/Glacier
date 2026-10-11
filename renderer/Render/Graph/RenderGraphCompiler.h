#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "Render/Base/Enums.h"

namespace glacier {
namespace render {

// The CPU half of the render graph (see .docs/render-graph-plan.md §4.4): it
// takes what the passes declared and works out the execution order, the passes
// nothing needs, the barrier plan and the transient plan. It knows handles,
// access bits and names - no driver, no texture, no command buffer - so it can
// be built and exercised on its own (build/probe/graph_test.cpp).

// One declared use of a resource inside a pass. The access bit is the state
// the pass wants the resource in while it runs.
struct GraphUsageDesc {
    uint32_t resource = 0;
    ResourceAccessBit access = ResourceAccessBit::kCommon;
};

struct GraphPassDesc {
    std::string name;
    // A pass whose output the outside world consumes - a present blit, a
    // readback - is a root of the cull (see §4.4 item 2).
    bool side_effect = false;
    std::vector<GraphUsageDesc> usages;  // as declared, duplicates and all
};

struct GraphResourceDesc {
    std::string name;
    bool imported = true;
    bool fixed_state = false;   // transitions on it are rejected by the runtime
    bool external = false;      // written to be read outside the graph
    ResourceAccessBit initial_state = ResourceAccessBit::kCommon;
    ResourceAccessBit final_state = ResourceAccessBit::kPixelShaderRead;
};

struct GraphCompileOptions {
    // Cull the passes no root reaches. Off by default while the migration is in
    // progress (see §4.4 item 2, open question 4): the order it produces is the
    // thing being compared against the registration order until then.
    bool cull = false;
};

// One transition of the plan: the resource goes from the state the pass before
// it left behind to the state this pass wants. Informational at execution time
// - the tracker knows the real state there - but it is what Dump prints and
// what the compiler tests pin down.
struct GraphBarrier {
    uint32_t resource = 0;
    ResourceAccessBit before = ResourceAccessBit::kCommon;
    ResourceAccessBit after = ResourceAccessBit::kCommon;
};

struct CompiledPass {
    uint32_t pass = 0;  // index into GraphCompileDesc::passes
    // What the pass may touch while it runs: one entry per resource, the reads
    // of a resource merged into one state. This is the set the executor makes
    // the pass's resources hold before it starts, and what a transition taken
    // by the pass body itself is checked against (§4.6).
    std::vector<GraphUsageDesc> expected;
    // The transitions the plan needs, in resource order.
    std::vector<GraphBarrier> barriers;
};

// Where a transient is first and last used, as positions of CompiledGraph::passes.
struct TransientUsage {
    uint32_t resource = 0;
    uint32_t first_pass = 0;
    uint32_t last_pass = 0;
};

struct CompiledGraph {
    bool ok = false;
    std::string error;

    std::vector<CompiledPass> passes;          // in execution order
    std::vector<uint32_t> culled;              // pass indices the cull dropped
    std::vector<GraphBarrier> final_barriers;  // resources back to final_state
    std::vector<TransientUsage> transients;
};

struct GraphCompileDesc {
    std::vector<GraphPassDesc> passes;
    std::vector<GraphResourceDesc> resources;
    GraphCompileOptions options;
};

// The one entry point: an invalid description - a use of a resource that does
// not exist, a resource written two ways at once, a dependency cycle - comes
// back as !ok with the reason in error, not as an exception (RenderGraph raises
// the exception the engine catches, this layer stays a plain function).
CompiledGraph CompileRenderGraph(const GraphCompileDesc& desc);

} // namespace render
} // namespace glacier
