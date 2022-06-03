#pragma once

#include <memory>
#include "render/base/renderable.h"
#include "geometry.h"
#include "mesh.h"

struct aiMesh;

namespace glacier {
namespace render {

class MeshRenderer : public Renderable {
public:
    MeshRenderer() {}
    MeshRenderer(const std::shared_ptr<Mesh>& mesh, Material* material = nullptr);

    void OnAwake() override;
    void OnEnable() override;
    void OnDisable() override;

    void Render(GfxDriver* gfx, Material* mat = nullptr) const override;
    void Draw(GfxDriver* gfx) const override;

    void AddMesh(const std::shared_ptr<Mesh>& mesh);

    void OnDrawSelectedGizmos() override;
    void DrawInspector() override;

    const std::vector<std::shared_ptr<Mesh>>& meshes() const { return meshes_; }

private:
    //TODO: support specfied materials
    std::vector<std::shared_ptr<Mesh>> meshes_;
};

}
}
