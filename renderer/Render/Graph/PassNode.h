#pragma once

#include <string>
#include <vector>
#include <array>
#include <memory>
#include "PassExecutor.h"
#include "ResourceEntry.h"
#include "RenderGraph.h"
#include "Render/Base/Renderable.h"
#include "Core/Identifiable.h"

namespace glacier {
namespace render {

class Material;

class PassNode : public Identifiable<PassNode> {
public:
    PassNode(const char* name, PassExecutor* executor) noexcept;

    PassNode(const PassNode& other) = delete;
    PassNode(PassNode&& other) noexcept = default;
    virtual ~PassNode() = default;

    virtual void Execute(Renderer* renderer);

    void Render(Renderer* renderer, const std::vector<Renderable*>& objs, Material* mat = nullptr) const;
    void Render(Renderer* renderer, const Renderable* obj = nullptr, Material* mat = nullptr) const;

    void PreRender(Renderer* renderer) const;
    void PostRender(Renderer* renderer) const;

    void Reset();

    void Finalize() const;

    const std::string& name() const { return name_; }
    RasterState& raster_state() { return rs_; }

    bool active() const { return active_; }
    void active(bool enable) { active_ = enable; }

protected:
    bool active_ = true;
    std::string name_;
    RasterState rs_;

    std::unique_ptr<PassExecutor> executor_;
};

}
}
