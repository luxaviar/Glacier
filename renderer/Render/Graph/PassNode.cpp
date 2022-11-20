#include "PassNode.h"
#include <sstream>
#include "Geometry/Frustum.h"
#include "Render/Camera.h"
#include "Render/Renderer.h"

namespace glacier {
namespace render {

PassNode::PassNode(const char* name) noexcept :
    Identifiable<PassNode>(),
    name_(name)
{

}

PassNode::PassNode(const char* name, std::unique_ptr<PassExecutor>&& executor) noexcept :
    Identifiable<PassNode>(),
    name_(name), executor_(std::move(executor))
{

}

void PassNode::Reset() {
    //TODO:
}

void PassNode::Setup(Renderer* renderer) {

}

void PassNode::Execute(CommandBuffer* cmd_buffer) {
    if (executor_) {
        executor_->Execute(cmd_buffer, *this);
    }
    else {
        Render(cmd_buffer);
    }
}

void PassNode::Render(CommandBuffer* cmd_buffer, const std::vector<Renderable*>& objs, Material* mat) const {
    if (mat) {
        for (auto o : objs) {
            o->Render(cmd_buffer, mat);
        }
    }
    else {
        for (auto o : objs) {
            auto cur_mat = o->GetMaterial();
            if (cur_mat->HasPass(this)) {
                o->Render(cmd_buffer, cur_mat.get());
            }
        }
    }
}

void PassNode::Render(CommandBuffer* cmd_buffer, const Renderable* obj, Material* mat) const {
    if (!obj) return;

    if (obj->IsActive()) {
        obj->Render(cmd_buffer, mat);
    }
}

void PassNode::Finalize() const
{
}

}
}
