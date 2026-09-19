#pragma once

#include <memory>
#include <string>
#include <vector>
#include "render/base/renderable.h"
#include "Animation/Skeleton.h"
#include "geometry.h"

struct aiMesh;

namespace glacier {
namespace render {

class CommandBuffer;

class Mesh {
public:
    static InputLayoutDesc kDefaultLayout;
    //the bind pose of a skinned mesh: the joints and their weights of every
    //vertex, which is what the GPU skinning pass reads
    static InputLayoutDesc kSkinnedLayout;
    //a skinned mesh the GPU skinning pass wrote: the deformed vertex and the
    //position it had in the frame before, which is all a pass needs to draw it
    static InputLayoutDesc kSkinnedVertexLayout;

    //bone binding of a skinned mesh: the node it drives in the model's node
    //table; offset_matrix is the inverse bind matrix that maps a mesh space
    //vertex into the bone's local space
    struct Bone {
        //index of the joint in the model's node table, which is the skeleton's
        //bone order; kInvalidBoneIndex when the importer could not match the
        //joint of the asset to one of its nodes
        int32_t node = kInvalidBoneIndex;
        Matrix4x4 offset_matrix;
    };

    Mesh();
    Mesh(const VertexCollection& vertices, const IndexCollection& indices, bool recalculate_normals = false);
    //bones are resolved by the importer, so the mesh never looks a joint up by
    //name; the list has to hold one entry per bone of the assimp mesh
    Mesh(const aiMesh& mesh, const std::vector<Bone>& bones);
    Mesh(const std::shared_ptr<Buffer>& vertex_buffer, const std::shared_ptr<Buffer>& index_buffer);
    
    const AABB& bounds() const { return bounds_; }

    const std::string& name() const { return name_; }
    void name(const char* name) { name_ = name; }

    const VertexCollection& vertices() const { return vertices_; }
    const IndexCollection& indices() const { return indices_; }

    bool IsSkinned() const { return !bones_.empty(); }
    const std::vector<Bone>& bones() const { return bones_; }

    //the bind pose buffer of the mesh; the GPU skinning pass reads it and the
    //passes draw it when a mesh is not skinned by the GPU
    const std::shared_ptr<Buffer>& vertex_buffer() const { return vertex_buffer_; }
    Buffer* index_buffer() const { return index_buffer_.get(); }

    void Draw(CommandBuffer* cmd_buffer) const;
    //draws the mesh from a vertex buffer someone else filled (the skinned
    //vertices of a mesh, or the instances of a batched draw)
    void Draw(CommandBuffer* cmd_buffer, Buffer* vertex_buffer, size_t vertex_offset = 0) const;
    //draws the mesh once per instance of a batch, whose object data the vertex
    //shader reads from the instance buffer
    void DrawInstanced(CommandBuffer* cmd_buffer, uint32_t instance_count) const;

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
