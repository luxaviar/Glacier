#pragma once

#include <optional>
#include <filesystem>
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/material.h>
#include "MeshRenderer.h"


struct aiNode;
struct aiScene;

namespace glacier {
namespace render {

class Model {
public:
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

        GameObject& GenerateGameObject(Transform* parent_tx, float scale);

    private:
        const Model* model_;
        Node* parent_ = nullptr;
        std::string name_;
        std::unique_ptr<Transform> transform_;
        std::vector<MeshDesc> meshes_;
        std::vector<Node> children_;
    };

    Model(const char* file, bool flip_uv=false);
    Model(const VertexCollection& vertices, const IndexCollection& indices, const std::shared_ptr<Material>& material);
    
    Node& root() { return root_; }
    const Node& root() const { return root_; }

    const std::string& name() const { return name_; }
    const aiScene* scene() const { return scene_; }

    const std::shared_ptr<Mesh>& GetMesh(size_t idx) const;
    const std::shared_ptr<Material>& GetMaterial(size_t idx) const;

    bool GetTexture(aiMaterial* mat, aiTextureType type, unsigned int index, aiString& value, aiTextureMapMode* mode) const;
    TextureDescription GetTextureDesc(const std::filesystem::path& base_path, aiMaterial* mtl,
        aiTextureType type, aiTextureType opt_type,
        const char* color_key, const Color& default_color,
        bool srgb, bool mips=true) const;

    GameObject& GenerateGameObject(float scale = 1.0f);
    static GameObject& GenerateGameObject(const char* file, bool flip_uv = false, float scale = 1.0f);

private:
    Node root_;
    Assimp::Importer importer_;
    const aiScene* scene_;
    std::string name_;
    std::vector<std::shared_ptr<Mesh>> meshes_;
    std::vector<std::shared_ptr<Material>> materials_;
};

}
}
