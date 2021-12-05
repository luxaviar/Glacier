#pragma once

#include <memory>
#include "render/base/renderable.h"
#include "geometry.h"

struct aiMesh;

namespace glacier {
namespace render {

class CommandList;

class Mesh {
public:
    Mesh();
    Mesh(const VertexCollection& vertices, const IndexCollection& indices, bool recalculate_normals = false);
    Mesh(const aiMesh& mesh);
    
    const AABB& bounds() const { return bounds_; }

    const std::string& name() const { return name_; }
    void name(const char* name) { name_ = name; }

    const VertexCollection& vertices() const { return vertices_; }
    const IndexCollection& indices() const { return indices_; }

    void Draw(GfxDriver* gfx) const;
    //void Draw(CommandList* cmd_list, uint32_t instance_count = 1, uint32_t start_instance = 0) const;

    void SetVertexBuffer(const VertexCollection& vertices, std::shared_ptr<VertexBuffer> vertices_buf);
    void SetIndexBuffer(const IndexCollection& indices, std::shared_ptr<IndexBuffer> indices_buf);

    void RecalculateNormals();

private:
    void Setup() const;
    void Bind(GfxDriver* gfx) const;
    inline Vec3f CalcRawNormalFromTriangle(const Vec3f& a, const Vec3f& b, const Vec3f& c)
    {
        return (b - a).Cross(c - a);
    }

protected:
    std::string name_;

    VertexCollection vertices_;
    IndexCollection indices_;

    mutable std::shared_ptr<VertexBuffer> vertices_buf_;
    mutable std::shared_ptr<IndexBuffer> indices_buf_;
    mutable std::shared_ptr<InputLayout> layout_;
    mutable AABB bounds_;
};

}
}
