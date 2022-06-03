#include "model.h"
#include "Render/Graph/PassNode.h"
#include "Common/Util.h"
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags
#include "Core/GameObject.h"

namespace glacier {
namespace render {

Model::Node::Node(Transform* tx, Node* parent, const aiNode& self, const Model& model) :
    parent_(parent),
    name_(self.mName.C_Str())
{
    transform_ = std::make_unique<Transform>(*(Matrix4x4*)(&self.mTransformation));
    transform_->SetParent(tx);

    AABB bound(Vec3f(FLT_MAX), Vec3f(-FLT_MAX));
    meshes_.reserve(self.mNumMeshes);
    for (size_t i = 0; i < self.mNumMeshes; ++i) {
        auto mesh = model.meshes_[self.mMeshes[i]].get();
        meshes_.push_back(self.mMeshes[i]);
        bound = AABB::Union(bound, mesh->bounds());
    }
    //it works, because the world space is identical to object space when model created at origin position.
    bounds_ = AABB::Transform(bound, transform_->LocalToWorldMatrix());

    children_.reserve(self.mNumChildren);
    for (size_t i = 0; i < self.mNumChildren; ++i) {
        auto child = self.mChildren[i];
        children_.emplace_back(transform_.get(), this, *child, model);
        //bounds_ = AABB::Union(bounds_, children_.back().bounds_);
    }
}

GameObject& Model::Node::GenerateGameObject(Transform* parent_tx, const Model* model, 
    Material* material, float scale) 
{
    auto& go = GameObject::Create(name_.c_str());
    if (!meshes_.empty()) {
        auto* mr = go.AddComponent<MeshRenderer>();
        mr->SetMaterial(material);
        for (auto i : meshes_) {
            mr->AddMesh(model->GetMesh(i));
        }
    }
    
    auto& tx = go.transform();
    tx.local_position(transform_->local_position());
    tx.local_rotation(transform_->local_rotation());
    if (!parent_tx) {
        tx.local_scale(transform_->local_scale() * scale);
    } else {
        tx.local_scale(transform_->local_scale());
        tx.SetParent(parent_tx);
    }

    for (auto& child : children_) {
        child.GenerateGameObject(&tx, model, material, scale);
    }
    return go;
}

Model::Model(const VertexCollection& vertices, const IndexCollection& indices) {
    auto m = std::make_shared<Mesh>(vertices, indices);
    meshes_.emplace_back(std::move(m));

    //root_.meshes_.push_back(meshes_[0].get());
    root_.meshes_.push_back(0);
    root_.parent_ = nullptr;
}

Model::Model(const char* file) {
    // Create an instance of the Importer class
    Assimp::Importer importer;

    // And have it read the given file with some example postprocessing
    // Usually - if speed is not the most important aspect for you - you'll
    // probably to request more postprocessing than we do in this example.
    const aiScene* scene = importer.ReadFile(file,
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_ConvertToLeftHanded |
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace);

    // If the import failed, report it
    if (!scene) {
        throw std::exception(importer.GetErrorString());
    }

    meshes_.reserve(scene->mNumMeshes);
    for (size_t i = 0; i < scene->mNumMeshes; i++) {
        const auto& mesh = *scene->mMeshes[i];
        auto mptr = std::make_shared<Mesh>(mesh);
        meshes_.emplace_back(std::move(mptr));
    }

    root_ = Node(nullptr, nullptr, *scene->mRootNode, *this);
}

std::shared_ptr<Mesh> Model::GetMesh(size_t idx) const {
    if (idx < meshes_.size()) {
        return meshes_[idx];
    }

    return {};
}

GameObject& Model::GenerateGameObject(Material* material, float scale) {
    return root_.GenerateGameObject(nullptr, this, material, scale);
}

GameObject& Model::GenerateGameObject(const char* file, Material* material, float scale) {
    Model model(file);
    return model.GenerateGameObject(material, scale);
}

}
}
