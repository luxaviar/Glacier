#include "mesh.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Render/Graph/PassNode.h"
#include "Common/Util.h"
#include "Render/Base/Indexbuffer.h"
#include "Render/Base/Vertexbuffer.h"
#include "Render/Base/Inputlayout.h"

namespace glacier {
namespace render {

Mesh::Mesh() : name_("unnamed") {}

Mesh::Mesh(const VertexCollection& vertices, const IndexCollection& indices, bool recalculate_normals) :
    Mesh()
{
    //Set(gfx, vertices, indices);
    vertices_ = vertices;
    indices_ = indices;

    if (recalculate_normals) {
        RecalculateNormals();
    }

    Setup();
}

Mesh::Mesh(const aiMesh& mesh) {
    name_ = mesh.mName.C_Str();

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

//void Mesh::CalcBounds() {
//    Vec3f min{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max() , std::numeric_limits<float>::max() };
//    Vec3f max{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() , std::numeric_limits<float>::lowest() };
//
//    for (auto& v : vertices_) {
//        min = Vec3f::Min(min, v.position);
//        max = Vec3f::Max(max, v.position);
//    }
//
//    bounds_ = AABB(min, max);
//}

void Mesh::Setup() const {
    InputLayoutDesc layout{ InputLayoutDesc::Position3D, InputLayoutDesc::Normal, InputLayoutDesc::Texture2D, InputLayoutDesc::Tangent };

    VertexData data(layout);
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

    vertices_buf_ = driver->CreateVertexBuffer(data);//  std::make_shared<Buffer>(*driver, data);
    indices_buf_ = driver->CreateIndexBuffer(indices_);
    layout_ = driver->CreateInputLayout(layout); // std::make_shared<InputLayout>(gfx, layout);
}

void Mesh::RecalculateNormals() {
    //std::fill_n(outNormals, vertexCount, Vector3f(0, 0, 0));
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

void Mesh::SetVertexBuffer(const VertexCollection& vertices, std::shared_ptr<VertexBuffer> vertices_buf) {
    vertices_ = vertices_;
    vertices_buf_ = vertices_buf;

    Vec3f min{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max() , std::numeric_limits<float>::max() };
    Vec3f max{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() , std::numeric_limits<float>::lowest() };

    for (auto& v : vertices_) {
        min = Vec3f::Min(min, v.position);
        max = Vec3f::Max(max, v.position);
    }

    bounds_ = AABB(min, max);
}

void Mesh::SetIndexBuffer(const IndexCollection& indices, std::shared_ptr<IndexBuffer> indices_buf) {
    indices_ = indices;
    indices_buf_ = indices_buf;
}

void Mesh::Bind(GfxDriver* gfx) const {
    if (!vertices_buf_) {
        Setup();
    }

    vertices_buf_->Bind();
    indices_buf_->Bind();
    if (layout_) gfx->UpdateInputLayout(layout_);
}

void Mesh::Draw(GfxDriver* gfx) const {
    Bind(gfx);
    gfx->DrawIndexed((uint32_t)indices_.size());
}

//void Mesh::Draw(CommandList* commandList, uint32_t instance_count, uint32_t start_instance) const {
//    commandList->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
//
//    //for (auto vertexBuffer : m_VertexBuffers)
//    //{
//        commandList->SetVertexBuffer(0, std::dynamic_pointer_cast<D3D12VertexBuffer>(vertices_buf_));
//    //}
//
//    auto indexCount = indices_.size();
//    auto vertexCount = vertices_.size();
//
//    if (indexCount > 0)
//    {
//        commandList->SetIndexBuffer(std::dynamic_pointer_cast<D3D12IndexBuffer>(indices_buf_));
//        commandList->DrawIndexed(indexCount, instance_count, 0u, 0u, start_instance);
//    }
//    else if (vertexCount > 0)
//    {
//        commandList->Draw(vertexCount, instance_count, 0u, start_instance);
//    }
//}

}
}
