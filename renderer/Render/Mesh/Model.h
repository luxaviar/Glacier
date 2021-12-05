#pragma once

#include "MeshRenderer.h"

struct aiNode;
struct aiScene;

namespace glacier {
namespace render {

class Model {
public:
    class Node {
    public:
        friend class Model;

        Node() {}
        Node(Transform* tx, Node* parent, const aiNode& self, const Model& model);

        std::vector<Node>& nodes() { return children_; }
        const std::vector<Node>& nodes() const { return children_; }

        std::vector<uint32_t>& meshes() { return meshes_; }
        const std::vector<uint32_t>& meshes() const { return meshes_; }

        const AABB& bounds() const { return bounds_; }
        const Transform& transform() const { return *transform_; }

        GameObject& GenerateGameObject(Transform* parent_tx, const Model* model, Material* material, float scale);

    private:
        Node* parent_ = nullptr;
        AABB bounds_;
        std::string name_;
        std::unique_ptr<Transform> transform_;
        //std::vector<Mesh*> meshes_;
        std::vector<uint32_t> meshes_;
        std::vector<Node> children_;
    };

    Model(const char* file);
    Model(const VertexCollection& vertices, const IndexCollection& indices);
    
    Node& root() { return root_; }
    const Node& root() const { return root_; }

    std::shared_ptr<Mesh> GetMesh(size_t idx) const;
    GameObject& GenerateGameObject(Material* material, float scale = 1.0f);

    static GameObject& GenerateGameObject(const char* file, Material* material, float scale = 1.0f);

private:
    Node root_;
    std::vector<std::shared_ptr<Mesh>> meshes_;
};

}
}
