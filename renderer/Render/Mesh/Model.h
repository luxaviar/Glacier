#pragma once

#include <optional>
#include <filesystem>
#include <unordered_map>
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/material.h>
#include "Animation/AnimationClip.h"
#include "Animation/NodeLookup.h"
#include "Animation/Skeleton.h"
#include "MeshRenderer.h"


struct aiNode;
struct aiScene;

namespace glacier {
namespace render {

class CommandBuffer;
class Texture;

class Model {
public:
    using TextureWarpMode = std::array<WarpMode, 3>;

    struct MeshDesc {
        uint32_t mesh;
        uint32_t material;
        //std::string name;
    };

    class Node {
    public:
        friend class Model;

        Node() {}
        Node(Transform* tx, Node* parent, const aiNode& self, const Model* model);

        std::vector<Node>& nodes() { return children_; }
        const std::vector<Node>& nodes() const { return children_; }

        std::vector<MeshDesc>& meshes() { return meshes_; }
        const std::vector<MeshDesc>& meshes() const { return meshes_; }

        //const AABB& bounds() const { return bounds_; }
        const Transform& transform() const { return *transform_; }

        //index of this node in the imported tree, which is also the index of its
        //bone in the skeleton
        uint32_t index() const { return index_; }

        //fills the transform of every node into `nodes`, which is what the
        //bones of the generated instance bind to
        GameObject& GenerateGameObject(Transform* parent_tx, float scale, const std::shared_ptr<NodeTransformTable>& nodes);

    private:
        const Model* model_;
        Node* parent_ = nullptr;
        uint32_t index_ = 0;
        std::string name_;
        std::unique_ptr<Transform> transform_;
        std::vector<MeshDesc> meshes_;
        std::vector<Node> children_;
    };

    Model(CommandBuffer* cmd_buffer, const char* file, bool flip_uv=false);
    Model(const VertexCollection& vertices, const IndexCollection& indices, const std::shared_ptr<Material>& material);
    
    Node& root() { return root_; }
    const Node& root() const { return root_; }

    const std::string& name() const { return name_; }

    const std::shared_ptr<Mesh>& GetMesh(size_t idx) const;
    const std::shared_ptr<Material>& GetMaterial(size_t idx) const;

    const std::vector<std::shared_ptr<AnimationClip>>& animations() const { return animations_; }
    //flat view of the imported node tree, with the skinning data of the meshes
    const std::shared_ptr<Skeleton>& skeleton() const { return skeleton_; }

    bool GetTexture(aiMaterial* mat, aiTextureType type, unsigned int index, aiString& value, aiTextureMapMode* mode) const;

    std::shared_ptr<Texture> LoadTexture(CommandBuffer* cmd_buffer, const std::filesystem::path& base_path, aiMaterial* mtl,
        aiTextureType type, aiTextureType opt_type, const char* color_key, const Color& default_color,
        bool srgb, bool mips, TextureWarpMode& mode) const;

    GameObject& CreateGameObject(float scale = 1.0f);

    //Loads a file and reuses the imported data when the same file was loaded
    //before: the meshes, materials, clips and the skeleton are shared between
    //every instance of the model. The entry is dropped when the source file
    //changed, so a reload still picks up an edited asset.
    static std::shared_ptr<Model> Load(CommandBuffer* cmd_buffer, const char* file, bool flip_uv = false);
    //releases every loaded model (their meshes, materials and textures)
    static void ClearCache();

    static GameObject& GenerateGameObject(CommandBuffer* cmd_buffer, const char* file, bool flip_uv = false, float scale = 1.0f);

private:
    friend class Node;

    //flat view of the imported node tree, in the order the skeleton stores its
    //bones in
    struct NodeInfo {
        const Node* node = nullptr;
        int32_t parent = kInvalidBoneIndex;
        //bind pose world matrix, used to tell nodes that share a name apart
        Matrix4x4 world_bind = Matrix4x4::identity;
        //inverse bind matrix the skin gives to the joint of this node
        Matrix4x4 inverse_bind = Matrix4x4::identity;
        bool has_inverse_bind = false;
    };

    static void CollectNodes(const Node& node, int32_t parent, const Matrix4x4& parent_world, std::vector<NodeInfo>& out);
    //distance between a joint's bind pose and a node's, 0 when they are the same
    static float BindError(const Matrix4x4& world_bind, const Matrix4x4& offset_matrix);

    //Maps every bone of the mesh to the node it drives: the name gives the
    //candidates and the inverse bind matrix picks among duplicated ones. Bones
    //that match no node are reported and stay at kInvalidBoneIndex.
    std::vector<Mesh::Bone> ResolveBones(const aiMesh& mesh, const std::string& file,
        std::vector<NodeInfo>& nodes,
        const std::unordered_map<std::string, std::vector<int32_t>>& joints) const;

    std::shared_ptr<Skeleton> BuildSkeleton(const std::vector<NodeInfo>& nodes) const;
    //loads the clips of the file from the cache or converts them, then binds them
    //to the skeleton of this model: the sampler addresses the bones by index and
    //only falls back to names for another skeleton
    void ImportAnimations(const std::filesystem::path& path);

    //mutable because the node tree numbers itself while it is being built
    mutable uint32_t node_count_ = 0;

    Node root_;
    std::unique_ptr<Assimp::Importer> importer_;
    //only valid while the model is built, see the end of the constructor
    const aiScene* scene_ = nullptr;
    std::string name_;
    std::vector<std::shared_ptr<Mesh>> meshes_;
    std::vector<std::shared_ptr<Material>> materials_;
    std::vector<std::shared_ptr<AnimationClip>> animations_;
    std::shared_ptr<Skeleton> skeleton_;
};

}
}
