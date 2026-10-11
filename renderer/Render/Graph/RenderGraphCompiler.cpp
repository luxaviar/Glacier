#include "RenderGraphCompiler.h"
#include <algorithm>
#include <functional>
#include <queue>

namespace glacier {
namespace render {
namespace {

constexpr uint32_t kNoPass = 0xFFFFFFFF;

// The access bits that write a resource. A write orders against everything
// else that touches the resource; kDepthWrite is a write even when the pass
// only tests against it, because that is the state the attachment puts it in.
constexpr uint64_t kWriteMask =
    (uint64_t)ResourceAccessBit::kRenderTarget |
    (uint64_t)ResourceAccessBit::kDepthWrite |
    (uint64_t)ResourceAccessBit::kShaderWrite |
    (uint64_t)ResourceAccessBit::kCopyDest |
    (uint64_t)ResourceAccessBit::kResolveDest;

bool IsWriteAccess(ResourceAccessBit access) {
    return ((uint64_t)access & kWriteMask) != 0;
}

void AddEdge(std::vector<std::vector<uint32_t>>& deps, uint32_t pass, uint32_t dep) {
    if (pass == dep) {
        return;
    }

    auto& edges = deps[pass];
    if (std::find(edges.begin(), edges.end(), dep) == edges.end()) {
        edges.push_back(dep);
    }
}

// Merges what a pass declared into one entry per resource. The reads of a
// resource merge into one state; two different writes to the same resource are
// a contradiction and stop the compile.
bool MergeUsages(const GraphCompileDesc& desc, const GraphPassDesc& pass, uint32_t pass_index,
    std::vector<GraphUsageDesc>& merged, std::string& error) {
    for (const auto& usage : pass.usages) {
        if (usage.resource >= desc.resources.size()) {
            error = "pass '" + pass.name + "' uses resource " + std::to_string(usage.resource) +
                ", which is not registered";
            return false;
        }

        if (usage.access == ResourceAccessBit::kCommon) {
            error = "pass '" + pass.name + "' uses '" + desc.resources[usage.resource].name +
                "' with no access";
            return false;
        }

        auto it = std::find_if(merged.begin(), merged.end(), [&](const GraphUsageDesc& other) {
            return other.resource == usage.resource;
        });

        if (it == merged.end()) {
            merged.push_back(usage);
            continue;
        }

        if (it->access == usage.access) {
            continue;
        }

        if (IsWriteAccess(it->access) && IsWriteAccess(usage.access)) {
            error = "pass '" + pass.name + "' uses '" + desc.resources[usage.resource].name +
                "' as two writes at once";
            return false;
        }

        if (IsWriteAccess(usage.access)) {
            it->access = usage.access;
        }
        else if (!IsWriteAccess(it->access)) {
            it->access = (ResourceAccessBit)((uint64_t)it->access | (uint64_t)usage.access);
        }
    }

    return true;
}

} // namespace

CompiledGraph CompileRenderGraph(const GraphCompileDesc& desc) {
    CompiledGraph graph;

    const uint32_t pass_count = (uint32_t)desc.passes.size();
    if (pass_count > 0xFFFFFFFE) {
        graph.error = "too many passes";
        return graph;
    }

    // 1. what each pass declared, merged to one use per resource
    std::vector<std::vector<GraphUsageDesc>> merged(pass_count);
    for (uint32_t p = 0; p < pass_count; ++p) {
        if (!MergeUsages(desc, desc.passes[p], p, merged[p], graph.error)) {
            return graph;
        }

        // resource order, so the plan of a pass is deterministic
        std::sort(merged[p].begin(), merged[p].end(), [](const GraphUsageDesc& a, const GraphUsageDesc& b) {
            return a.resource < b.resource;
        });
    }

    // 2. dependencies. A read belongs to the frame's producer of the resource:
    //    the writer registered before it, or - when it comes before every
    //    writer - the first writer, which is why registration order does not
    //    decide whether a pass consumes what another one wrote. A writer
    //    follows the writer before it and every reader it would overwrite the
    //    ground under. This is also the one place a cycle can come from: two
    //    passes that produce what the other consumes.
    std::vector<std::vector<uint32_t>> deps(pass_count);
    std::vector<std::vector<uint32_t>> dependents(pass_count);
    {
        struct UseState {
            uint32_t last_writer = kNoPass;
            std::vector<uint32_t> readers;        // since the last writer
            std::vector<uint32_t> early_readers;  // before the first writer
        };

        std::vector<UseState> uses(desc.resources.size());
        for (uint32_t p = 0; p < pass_count; ++p) {
            for (const auto& usage : merged[p]) {
                auto& use = uses[usage.resource];
                if (IsWriteAccess(usage.access)) {
                    if (use.last_writer == kNoPass) {
                        for (auto reader : use.early_readers) {
                            AddEdge(deps, reader, p);
                        }
                    }
                    else {
                        AddEdge(deps, p, use.last_writer);

                        for (auto reader : use.readers) {
                            AddEdge(deps, p, reader);
                        }

                        use.readers.clear();
                        use.early_readers.clear();
                    }

                    use.last_writer = p;
                }
                else {
                    if (std::find(use.readers.begin(), use.readers.end(), p) == use.readers.end()) {
                        use.readers.push_back(p);

                        if (use.last_writer == kNoPass) {
                            use.early_readers.push_back(p);
                        }
                    }

                    if (use.last_writer != kNoPass) {
                        AddEdge(deps, p, use.last_writer);
                    }
                }
            }
        }
    }

    for (uint32_t p = 0; p < pass_count; ++p) {
        for (auto dep : deps[p]) {
            dependents[dep].push_back(p);
        }
    }

    // 3. cull: a pass is kept when a root - a side effect pass or a pass that
    //    writes something the outside reads - reaches it through its dependencies
    std::vector<bool> kept(pass_count, !desc.options.cull);
    if (desc.options.cull) {
        std::vector<uint32_t> roots;
        for (uint32_t p = 0; p < pass_count; ++p) {
            if (desc.passes[p].side_effect) {
                roots.push_back(p);
                continue;
            }

            for (const auto& usage : merged[p]) {
                if (IsWriteAccess(usage.access) && desc.resources[usage.resource].external) {
                    roots.push_back(p);
                    break;
                }
            }
        }

        std::vector<uint32_t> stack = roots;
        for (auto root : roots) {
            kept[root] = true;
        }

        while (!stack.empty()) {
            auto p = stack.back();
            stack.pop_back();

            for (auto dep : deps[p]) {
                if (!kept[dep]) {
                    kept[dep] = true;
                    stack.push_back(dep);
                }
            }
        }

        for (uint32_t p = 0; p < pass_count; ++p) {
            if (!kept[p]) {
                graph.culled.push_back(p);
            }
        }
    }

    // 4. execution order: a stable topological sort - among the passes that are
    //    ready the one registered first runs first, so an unchanged project
    //    keeps running what it ran, and a difference can only come from a real
    //    dependency (§4.4 item 3)
    std::vector<uint32_t> order;
    {
        std::vector<uint32_t> indegree(pass_count, 0);
        for (uint32_t p = 0; p < pass_count; ++p) {
            if (!kept[p]) {
                continue;
            }

            for (auto dep : deps[p]) {
                if (kept[dep]) {
                    ++indegree[p];
                }
            }
        }

        std::priority_queue<uint32_t, std::vector<uint32_t>, std::greater<uint32_t>> ready;
        for (uint32_t p = 0; p < pass_count; ++p) {
            if (kept[p] && indegree[p] == 0) {
                ready.push(p);
            }
        }

        while (!ready.empty()) {
            auto p = ready.top();
            ready.pop();
            order.push_back(p);

            for (auto dependent : dependents[p]) {
                if (kept[dependent] && --indegree[dependent] == 0) {
                    ready.push(dependent);
                }
            }
        }

        uint32_t kept_count = 0;
        for (uint32_t p = 0; p < pass_count; ++p) {
            kept_count += kept[p] ? 1u : 0u;
        }

        if (order.size() != kept_count) {
            graph.error = "dependency cycle among passes:";
            uint32_t listed = 0;
            for (uint32_t p = 0; p < pass_count; ++p) {
                if (kept[p] && indegree[p] > 0) {
                    graph.error += (listed++ == 0 ? " " : ", ") + desc.passes[p].name;
                    if (listed == 8) {
                        graph.error += ", ...";
                        break;
                    }
                }
            }

            return graph;
        }
    }

    // 5. barrier plan: walk the order, remember the state each resource is left
    //    in, and record a transition wherever a pass wants another one
    std::vector<ResourceAccessBit> state(desc.resources.size());
    for (size_t r = 0; r < desc.resources.size(); ++r) {
        state[r] = desc.resources[r].initial_state;
    }

    for (auto p : order) {
        CompiledPass compiled_pass;
        compiled_pass.pass = p;
        compiled_pass.expected = merged[p];

        for (const auto& usage : merged[p]) {
            if (desc.resources[usage.resource].fixed_state) {
                continue;
            }

            if (state[usage.resource] != usage.access) {
                compiled_pass.barriers.push_back({ usage.resource, state[usage.resource], usage.access });
                state[usage.resource] = usage.access;
            }
        }

        graph.passes.push_back(std::move(compiled_pass));
    }

    for (size_t r = 0; r < desc.resources.size(); ++r) {
        if (desc.resources[r].fixed_state || state[r] == desc.resources[r].final_state) {
            continue;
        }

        graph.final_barriers.push_back({ (uint32_t)r, state[r], desc.resources[r].final_state });
    }

    // 6. transient plan: Phase 1 hands a transient out for the whole frame (see
    //    §4.4 item 5), so all that is needed is where it is used at all
    for (size_t r = 0; r < desc.resources.size(); ++r) {
        if (desc.resources[r].imported) {
            continue;
        }

        TransientUsage transient;
        transient.resource = (uint32_t)r;
        transient.first_pass = kNoPass;

        for (uint32_t oi = 0; oi < graph.passes.size(); ++oi) {
            for (const auto& usage : graph.passes[oi].expected) {
                if (usage.resource != r) {
                    continue;
                }

                if (transient.first_pass == kNoPass) {
                    transient.first_pass = oi;
                }

                transient.last_pass = oi;
            }
        }

        if (transient.first_pass != kNoPass) {
            graph.transients.push_back(transient);
        }
    }

    graph.ok = true;
    return graph;
}

} // namespace render
} // namespace glacier
