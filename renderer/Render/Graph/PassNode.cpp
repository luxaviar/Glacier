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
        RenderList(cmd_buffer, objs, mat);
        return;
    }

    //objects that bring their own material can only share a batch while the
    //material stays the same, so the list is cut at every material change
    std::vector<Renderable*> group;
    Material* group_mat = nullptr;

    for (auto o : objs) {
        auto& cur_mat = o->GetMaterial();
        if (!cur_mat || !cur_mat->HasPass(this)) {
            if (!group.empty()) {
                RenderList(cmd_buffer, group, group_mat);
                group.clear();
                group_mat = nullptr;
            }
            continue;
        }

        if (group_mat && group_mat != cur_mat.get()) {
            RenderList(cmd_buffer, group, group_mat);
            group.clear();
        }

        group_mat = cur_mat.get();
        group.push_back(o);
    }

    if (!group.empty()) {
        RenderList(cmd_buffer, group, group_mat);
    }
}

void PassNode::RenderList(CommandBuffer* cmd_buffer, const std::vector<Renderable*>& objs, Material* mat) {
    std::vector<Renderable*> batch;

    //a batch is drawn by its first object
    auto flush = [&]() {
        if (batch.empty()) {
            return;
        }

        if (batch.size() > 1) {
            batch.front()->RenderBatch(cmd_buffer, batch, mat);
        }
        else {
            batch.front()->Render(cmd_buffer, mat);
        }

        batch.clear();
    };

    for (auto o : objs) {
        //the batch is cut as soon as an object cannot share the draw
        if (!batch.empty() && !batch.front()->CanBatchWith(o, mat)) {
            flush();
        }

        batch.push_back(o);
    }

    flush();
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
