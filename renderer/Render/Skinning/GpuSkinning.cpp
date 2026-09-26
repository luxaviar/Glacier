#include "GpuSkinning.h"
#include "BoneMatrixPool.h"
#include "Math/Util.h"
#include "Render/Material.h"
#include "Render/Mesh/Mesh.h"
#include "Render/Base/CommandBuffer.h"
#include "Render/Base/Program.h"
#include "Common/Log.h"

namespace glacier {
namespace render {

void GpuSkinning::Setup() {
    if (material_) {
        return;
    }

    auto gfx = GfxDriver::Get();

    uint32_t stride = (uint32_t)Mesh::kSkinnedVertexLayout.data_size();
    //the capacity is a budget of memory, so it is only the number of vertices
    //it is meant to be for the layout it was divided by
    assert(stride == kVertexSize);
    vertex_buffer_ = gfx->CreateVertexBuffer(stride * kCapacity, stride, CreateFlags::kUav);
    vertex_buffer_->SetName("skinned vertex pool");

    material_ = std::make_shared<Material>("skinning", TEXT("SkinningCS"));
    skin_params_ = gfx->CreateConstantParameter<SkinParams, UsageType::kDynamic>();

    material_->SetProperty("_SkinningParams", skin_params_);
    material_->SetProperty("_SkinnedVertices", vertex_buffer_);
}

void GpuSkinning::Release() {
    material_.reset();
    vertex_buffer_.reset();
    regions_.clear();
}

uint32_t GpuSkinning::Allocate(const Mesh* mesh) {
    Setup();

    uint32_t count = (uint32_t)mesh->vertices().size();
    uint32_t used = 0;
    for (auto& region : regions_) {
        used = std::max(used, region.first + region.second);
    }

    if (count == 0 || used + count > kCapacity) {
        LOG_ERR("skinned vertex pool is full: {0} of {1} vertices are in use, a mesh of {2} vertices is skinned by the vertex shader",
            used, kCapacity, count);
        return kInvalidOffset;
    }

    regions_.emplace_back(used, count);

    LOG_LOG("skinned vertex pool: {0} of {1} vertices in use, {2} for mesh '{3}'",
        used + count, kCapacity, count, mesh->name());

    return used;
}

void GpuSkinning::Free(uint32_t offset) {
    if (offset == kInvalidOffset) {
        return;
    }

    regions_.erase(std::remove_if(regions_.begin(), regions_.end(),
        [offset](const std::pair<uint32_t, uint32_t>& region) { return region.first == offset; }),
        regions_.end());
}

void GpuSkinning::Dispatch(CommandBuffer* cmd_buffer, const Mesh* mesh, uint32_t vertex_offset,
    uint32_t bone_offset, uint32_t prev_bone_offset)
{
    if (!material_ || vertex_offset == kInvalidOffset || bone_offset == kInvalidBoneOffset) {
        return;
    }

    uint32_t vertex_count = (uint32_t)mesh->vertices().size();
    skin_params_ = SkinParams{ vertex_count, vertex_offset, bone_offset, prev_bone_offset };

    //The mesh, the pose and the region of the pool change with every mesh while
    //the material stays the same, and binding a material that did not change
    //does not bind its resources again (CommandBuffer::BindMaterial), so the
    //dispatch starts from no material at all.
    material_->SetProperty("_BindPoseVertices", mesh->vertex_buffer());
    material_->SetProperty("_BoneMatrices", BoneMatrixPool::Instance()->buffer());
    material_->SetProperty("_SkinningParams", skin_params_);

    cmd_buffer->UnBindMaterial();
    cmd_buffer->BindMaterial(material_.get());

    cmd_buffer->Dispatch(math::DivideByMultiple(vertex_count, kThreads), 1, 1);
    //the passes fetch the vertices the dispatch just wrote
    cmd_buffer->UavResource(vertex_buffer_.get());
}

}
}
