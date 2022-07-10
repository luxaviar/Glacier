#pragma once

#include <memory>
#include "render/base/renderable.h"
#include "geometry.h"
#include "mesh.h"

struct aiMesh;
class CommandBuffer;

namespace glacier {
namespace render {

class MeshRenderer : public Renderable {
public:
    MeshRenderer() {}
    MeshRenderer(const std::shared_ptr<Mesh>& mesh, const std::shared_ptr<Material>& material = {});

    void OnAwake() override;
    void OnEnable() override;
    void OnDisable() override;

    void Render(CommandBuffer* cmd_buffer, Material* mat = nullptr) const override;
    void Draw(CommandBuffer* cmd_buffer) const override;

    void AddMesh(const std::shared_ptr<Mesh>& mesh);

    void OnDrawSelectedGizmos() override;
    void DrawInspector() override;

    const std::vector<std::shared_ptr<Mesh>>& meshes() const { return meshes_; }

private:
    std::vector<std::shared_ptr<Mesh>> meshes_;
};

}
}
