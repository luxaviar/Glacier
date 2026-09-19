#include "mesh.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Render/Graph/PassNode.h"
#include "Common/Util.h"
#include "Render/Base/buffer.h"
#include "Render/Base/Inputlayout.h"
#include "Render/Base/CommandBuffer.h"
#include "Render/Base/CommandQueue.h"
#include "Common/Log.h"

namespace glacier {
namespace render {

InputLayoutDesc Mesh::kDefaultLayout = InputLayoutDesc{ InputLayoutDesc::Position3D, InputLayoutDesc::Normal, InputLayoutDesc::Texture2D, InputLayoutDesc::Tangent };
InputLayoutDesc Mesh::kSkinnedLayout = InputLayoutDesc{ InputLayoutDesc::Position3D, InputLayoutDesc::Normal, InputLayoutDesc::Texture2D, InputLayoutDesc::Tangent,
    InputLayoutDesc::BoneWeights, InputLayoutDesc::BoneIndices };

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

Mesh::Mesh(const aiMesh& mesh, const std::vector<Bone>& bones) {
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

    bones_ = bones;
    ImportBones(mesh);

    Setup();
}

void Mesh::ImportBones(const aiMesh& mesh) {
    if (mesh.mNumBones == 0) return;

    //the bones and their node bindings come from the importer, one entry per
    //bone of the assimp mesh and in the same order
    ASSERT(bones_.size() == mesh.mNumBones);

    //keep the four strongest influences of every vertex, then normalize them
    constexpr size_t kMaxInfluence = 4;

    struct Influence {
        float weight = 0.0f;
        uint32_t bone = 0;
    };

    std::vector<std::array<Influence, kMaxInfluence>> influences(vertices_.size());

    for (size_t b = 0; b < bones_.size(); ++b) {
        const auto& bone = *mesh.mBones[b];
        for (size_t w = 0; w < bone.mNumWeights; ++w) {
            const auto& weight = bone.mWeights[w];
            if (weight.mVertexId >= vertices_.size()) continue;
            if (b >= kMaxBones) continue; //beyond the shader limit, drop the influence

            auto& slots = influences[weight.mVertexId];
            size_t weakest = 0;
            for (size_t s = 1; s < kMaxInfluence; ++s) {
                if (slots[s].weight < slots[weakest].weight) {
                    weakest = s;
                }
            }

            if (weight.mWeight > slots[weakest].weight) {
                slots[weakest] = Influence{ weight.mWeight, (uint32_t)b };
            }
        }
    }

    for (size_t v = 0; v < vertices_.size(); ++v) {
        auto& slots = influences[v];

        float total = 0.0f;
        for (auto& slot : slots) {
            total += slot.weight;
        }

        if (total <= 0.0f) {
            //unweighted vertices are bound to the first bone so that they cannot collapse
            vertices_[v].bone_indices = Vec4u(0u, 0u, 0u, 0u);
            vertices_[v].bone_weights = Vec4f(1.0f, 0.0f, 0.0f, 0.0f);
            continue;
        }

        float inv_total = 1.0f / total;
        vertices_[v].bone_indices = Vec4u(slots[0].bone, slots[1].bone, slots[2].bone, slots[3].bone);
        vertices_[v].bone_weights = Vec4f(
            slots[0].weight * inv_total, slots[1].weight * inv_total,
            slots[2].weight * inv_total, slots[3].weight * inv_total);
    }
}

Mesh::Mesh(const std::shared_ptr<Buffer>& vertex_buffer, const std::shared_ptr<Buffer>& index_buffer) :
    vertex_buffer_(vertex_buffer),
    index_buffer_(index_buffer)
{

}

void Mesh::Setup() {
    bool skinned = IsSkinned();
    VertexData data(skinned ? kSkinnedLayout : kDefaultLayout);
    data.Reserve(vertices_.size());

    Vec3f min{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max() , std::numeric_limits<float>::max() };
    Vec3f max{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() , std::numeric_limits<float>::lowest() };

    for (auto& v : vertices_) {
        if (skinned) {
            data.EmplaceBack(v.position, v.normal, v.texcoord, v.tangent, v.bone_weights, v.bone_indices);
        }
        else {
            data.EmplaceBack(v.position, v.normal, v.texcoord, v.tangent);
        }

        min = Vec3f::Min(min, v.position);
        max = Vec3f::Max(max, v.position);
    }

    bounds_ = AABB(min, max);

    auto driver = GfxDriver::Get();

    vertex_buffer_ = driver->CreateVertexBuffer(data.data_size(), data.stride());
    vertex_buffer_->SetName("vertex buffer");

    index_buffer_ = driver->CreateIndexBuffer(indices_.size() * sizeof(uint32_t), IndexFormat::kUInt32);
    index_buffer_->SetName("index buffer");

    auto cmd_queue = driver->GetCommandQueue(CommandBufferType::kDirect);
    auto cmd_buffer = cmd_queue->GetCommandBuffer();

    vertex_buffer_->Upload(cmd_buffer, data.data(), data.data_size());
    index_buffer_->Upload(cmd_buffer, indices_.data(), indices_.size() * sizeof(uint32_t));

    cmd_queue->ExecuteCommandBuffer(cmd_buffer);
    cmd_queue->Flush();
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

void Mesh::Bind(CommandBuffer* cmd_buffer) const {
    vertex_buffer_->Bind(cmd_buffer);
    index_buffer_->Bind(cmd_buffer);
}

void Mesh::Draw(CommandBuffer* cmd_buffer) const {
    Bind(cmd_buffer);
    cmd_buffer->DrawIndexedInstanced((uint32_t)index_buffer_->count(), 1, 0, 0, 0);
}

}
}
