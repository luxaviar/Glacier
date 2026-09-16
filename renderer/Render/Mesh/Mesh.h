#pragma once

#include <memory>
#include <string>
#include <vector>
#include "render/base/renderable.h"
#include "geometry.h"

struct aiMesh;

namespace glacier {
namespace render {

class CommandBuffer;

class Mesh {
public:
    static InputLayoutDesc kDefaultLayout;
    static InputLayoutDesc kSkinnedLayout;

    //bone binding of a skinned mesh; offset_matrix is the inverse bind matrix
    //that maps a mesh space vertex into the bone's local space
    struct Bone {
        std::string name;
        Matrix4x4 offset_matrix;
    };

    Mesh();
    Mesh(const VertexCollection& vertices, const IndexCollection& indices, bool recalculate_normals = false);
    Mesh(const aiMesh& mesh);
    Mesh(const std::shared_ptr<Buffer>& vertex_buffer, const std::shared_ptr<Buffer>& index_buffer);
    
    const AABB& bounds() const { return bounds_; }

    const std::string& name() const { return name_; }
    void name(const char* name) { name_ = name; }

    const VertexCollection& vertices() const { return vertices_; }
    const IndexCollection& indices() const { return indices_; }

    bool IsSkinned() const { return !bones_.empty(); }
    const std::vector<Bone>& bones() const { return bones_; }

    Buffer* vertex_buffer() const { return vertex_buffer_.get(); }
    Buffer* index_buffer() const { return index_buffer_.get(); }

    void Draw(CommandBuffer* cmd_buffer) const;

    void RecalculateNormals();

private:
    void Setup();
    void ImportBones(const aiMesh& mesh);

    void Bind(CommandBuffer* cmd_buffer) const;

    inline Vec3f CalcRawNormalFromTriangle(const Vec3f& a, const Vec3f& b, const Vec3f& c) {
        return (b - a).Cross(c - a);
    }

protected:
    std::string name_;

    VertexCollection vertices_;
    IndexCollection indices_;
    std::vector<Bone> bones_;

    std::shared_ptr<Buffer> vertex_buffer_;
    std::shared_ptr<Buffer> index_buffer_;
    AABB bounds_;
};

}
}
