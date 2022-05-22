#include "mesh.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Render/Graph/PassNode.h"
#include "Common/Util.h"
#include "Render/Base/buffer.h"
#include "Render/Base/Inputlayout.h"

namespace glacier {
namespace render {

InputLayoutDesc Mesh::kDefaultLayout = InputLayoutDesc{ InputLayoutDesc::Position3D, InputLayoutDesc::Normal, InputLayoutDesc::Texture2D, InputLayoutDesc::Tangent };

Mesh::Mesh() : name_("unnamed") {}

Mesh::Mesh(const VertexCollection& vertices, const IndexCollection& indices, bool recalculate_normals) :
    Mesh()
{
    vertices_ = vertices;
    indices_ = indices;

    if (recalculate_normals) {
        RecalculateNormals();
    }

    Setup();
}

Mesh::Mesh(const aiMesh& mesh) {
    name_ = mesh.mName.C_Str();

    vertices_.reserve(mesh.mNumVertices);
    indices_.reserve(mesh.mNumFaces * 3);

    auto texcoords = mesh.mTextureCoords ? mesh.mTextureCoords[0] : nullptr;
    for (size_t i = 0; i < mesh.mNumVertices; ++i) {
        const auto& position = mesh.mVertices[i];
        const auto& normal = mesh.mNormals[i];
        Vec2f texcoord{ 0.0f };
        Vec3f tangent{ 0.0f };
        Vec3f bitangent{ 0.0f };
        if (texcoords) texcoord = { texcoords[i].x, texcoords[i].y };
        if (mesh.mTangents) tangent = { mesh.mTangents[i].x, mesh.mTangents[i].y, mesh.mTangents[i].z };
        if (mesh.mBitangents) bitangent = { mesh.mBitangents[i].x, mesh.mBitangents[i].y, mesh.mBitangents[i].z };
        
        vertices_.emplace_back(
            Vec3f{ position.x, position.y, position.z },
            Vec3f{ normal.x, normal.y, normal.z },
            texcoord,
            tangent,
            bitangent
        );
    }

    for (size_t i = 0; i < mesh.mNumFaces; ++i) {
        const auto& face = mesh.mFaces[i];
        for (size_t j = 0; j < face.mNumIndices; ++j) {
            indices_.push_back(face.mIndices[j]);
        }
    }

    Setup();
}

Mesh::Mesh(const std::shared_ptr<VertexBuffer>& vertex_buffer, const std::shared_ptr<IndexBuffer>& index_buffer) :
    vertex_buffer_(vertex_buffer),
    index_buffer_(index_buffer)
{

}

void Mesh::Setup() {
    VertexData data(kDefaultLayout);
    data.Reserve(vertices_.size());

    Vec3f min{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max() , std::numeric_limits<float>::max() };
    Vec3f max{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() , std::numeric_limits<float>::lowest() };

    for (auto& v : vertices_) {
        data.EmplaceBack(v.position, v.normal, v.texcoord, v.tangent);
        min = Vec3f::Min(min, v.position);
        max = Vec3f::Max(max, v.position);
    }

    bounds_ = AABB(min, max);

    auto driver = GfxDriver::Get();

    vertex_buffer_ = driver->CreateVertexBuffer(data);
    vertex_buffer_->SetName(TEXT("vertex buffer"));

    index_buffer_ = driver->CreateIndexBuffer(indices_);
    index_buffer_->SetName(TEXT("index buffer"));
}

void Mesh::RecalculateNormals() {
    for (auto& vertex : vertices_) {
        vertex.normal = Vec3f::zero;
    }

    // Add normals from faces
    int idx = 0;
    for (int i = 0; i < indices_.size() / 3; ++i)
    {
        uint32_t index0 = indices_[idx + 0];
        uint32_t index1 = indices_[idx + 1];
        uint32_t index2 = indices_[idx + 2];
        Vector3 faceNormal = CalcRawNormalFromTriangle(vertices_[index0].position, vertices_[index1].position, vertices_[index2].position);
        vertices_[index0].normal += faceNormal;
        vertices_[index1].normal += faceNormal;
        vertices_[index2].normal += faceNormal;
        idx += 3;
    }

    // Normalize
    for (auto& vertex : vertices_) {
        vertex.normal.Normalize();
    }
}

void Mesh::Bind(GfxDriver* gfx) const {
    vertex_buffer_->Bind();
    index_buffer_->Bind();
}

void Mesh::Draw(GfxDriver* gfx) const {
    Bind(gfx);
    gfx->DrawIndexed((uint32_t)index_buffer_->count());
}

}
}
