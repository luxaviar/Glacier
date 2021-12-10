#include "PassNode.h"
#include <sstream>
#include "Geometry/Frustum.h"
#include "Render/Camera.h"
#include "Render/Renderer.h"

namespace glacier {
namespace render {

PassNode::PassNode(const char* name, PassExecutor* executor) noexcept :
    Identifiable<PassNode>(),
    name_(name), executor_(executor)
{

}

void PassNode::Reset() {
    //TODO:
}

void PassNode::PreRender(Renderer* renderer) const {

}

void PassNode::PostRender(Renderer* renderer) const {

}

void PassNode::Execute(Renderer* renderer) {
    if (executor_) {
        executor_->Execute(renderer, *this);
    } else {
        Render(renderer);
    }
}

void PassNode::Render(Renderer* renderer, const std::vector<Renderable*>& objs, Material* mat) const {
    auto gfx = renderer->driver();

    PreRender(renderer);

    if (mat) {
        MaterialGuard guard(gfx, mat);
        for (auto o : objs) {
            o->Render(gfx);
        }
    } else {
        Material* last_mat = nullptr;
        int n = 0;
        for (auto o : objs) {
            auto cur_mat = o->GetMaterial();
            if (cur_mat->HasPass(this)) {
                last_mat = cur_mat;
                if (gfx->PushMaterial(last_mat)) {
                    ++n;
                }
                o->Render(gfx);
            }
        }
        gfx->PopMaterial(n);
    }

    PostRender(renderer);
}

void PassNode::Render(Renderer* renderer, const Renderable* obj, Material* mat) const {
    if (!obj) return;

    PreRender(renderer);

    if (obj->IsActive()) {
        auto gfx = renderer->driver();
        MaterialGuard guard(gfx, mat);
        obj->Render(gfx);
    }

    PostRender(renderer);
}

void PassNode::Finalize() const
{
}

}
}
