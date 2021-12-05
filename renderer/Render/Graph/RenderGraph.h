#pragma once
#include <string>
#include <vector>
#include <memory>
#include "Exception/Exception.h"
#include "PassExecutor.h"
#include "ResourceEntry.h"

namespace glacier {
namespace render {

class GfxDriver;
class RenderTarget;
class PassNode;
class RenderPass;
class Renderer;

class RenderGraphCompileException : public BaseException {
public:
    RenderGraphCompileException(int line, const TCHAR* file, std::string desc) noexcept;
    const TCHAR* type() const noexcept override;
};

#define RGC_EXCEPTION(message) RenderGraphCompileException(__LINE__, TEXT(__FILE__), (message))

class RenderGraph {
public:
    RenderGraph();
    ~RenderGraph();

    RenderGraph(const RenderGraph&) = delete;

    void Execute(Renderer* renderer);
    void Reset() noexcept;

    PassNode& GetPass(const char* passName);

    template<typename Data=nullptr_t, typename Setup, typename Execute>
    PassNode& AddPass(const char* name, Setup setup, Execute&& execute) {
        auto executor = new RenderPassExecutor<Data, Execute>(std::forward<Execute>(execute));
        auto& pass = CreatePass(name, executor);

        if constexpr (std::is_same<Data, nullptr_t>::value) {
            setup(pass);
        }
        else {
            setup(executor->data(), pass);
        }

        return pass;
    }

    template<typename Setup>
    PassNode& AddPass(const char* name, Setup setup) {
        auto& pass = CreatePass(name, nullptr);
        setup(pass);
        return pass;
    }

    template<typename T, typename ...Args>
    ResourceHandle<T> Create(const char* name, Args&&... args) {
        std::shared_ptr<T> p = std::make_shared<T>(std::forward<Args>(args)...);
        uint32_t id = (uint32_t)resources_.size();
        auto sp = std::make_unique<ResourceEntry<T>>(id, name, std::move(p));
        resources_.emplace_back(std::move(sp));
        return ResourceHandle<T>(id);
    }

    template<typename T>
    ResourceEntry<T>& Get(ResourceHandle<T> handle) {
        assert(handle.id < resources_.size());
        auto ptr = resources_[handle.id].get();
        ResourceEntry<T>* ret = dynamic_cast<ResourceEntry<T>*>(ptr);
        assert(ret);
        return *ret;
    }

    ResourceEntryBase& Get(ResourceID ri) {
        assert(ri.id < resources_.size());
        auto ptr = resources_[ri.id].get();
        return *ptr;
    }

    void Compile();

protected:
    PassNode& CreatePass(const char* name, PassExecutor* executor);

    bool baked = false;
    std::vector<PassNode> passes_;
    std::vector<std::unique_ptr<ResourceEntryBase>> resources_;
};

}
}
