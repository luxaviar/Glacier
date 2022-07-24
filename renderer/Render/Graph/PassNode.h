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
class CommandBuffer;

class PassNode : public Identifiable<PassNode> {
public:
    PassNode(const char* name, PassExecutor* executor) noexcept;

    PassNode(const PassNode& other) = delete;
    PassNode(PassNode&& other) noexcept = default;
    virtual ~PassNode() = default;

    virtual void Execute(CommandBuffer* cmd_buffer);

    void Render(CommandBuffer* cmd_buffer, const std::vector<Renderable*>& objs, Material* mat = nullptr) const;
    void Render(CommandBuffer* cmd_buffer, const Renderable* obj = nullptr, Material* mat = nullptr) const;

    void Reset();

    void Finalize() const;

    const std::string& name() const { return name_; }

    bool active() const { return active_; }
    void active(bool enable) { active_ = enable; }

protected:
    bool active_ = true;
    std::string name_;

    std::unique_ptr<PassExecutor> executor_;
};

}
}
