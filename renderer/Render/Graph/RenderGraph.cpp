#include "RenderGraph.h"
#include "PassNode.h"
#include <sstream>

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
    return passes_.back();
}

void RenderGraph::Execute(CommandBuffer* cmd_buffer) {
    assert(baked);
    for (auto& p : passes_) {
        if (p.active()) {
            p.Execute(cmd_buffer);
        }
    }
}

void RenderGraph::Reset() noexcept
{
    assert(baked);
    for( auto& p : passes_)
    {
        p.Reset();
    }
}

void RenderGraph::Compile() {
    assert(!baked);
    baked = true;
    //for( const auto& p : passes_)
    //{
    //    p.Finalize();
    //}
    //finalized = true;
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

}
}
