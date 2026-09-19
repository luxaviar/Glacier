#include "BoneMatrixPool.h"
#include "Render/Base/Buffer.h"
#include "Render/Base/GfxDriver.h"
#include "Common/Log.h"

namespace glacier {
namespace render {

static_assert(BoneMatrixPool::kRegionCount == GfxDriver::kBufferCount,
    "the bone matrices have to stay valid until the frame that used them was presented");

void BoneMatrixPool::Setup() {
    if (buffer_) {
        return;
    }

    buffer_ = GfxDriver::Get()->CreateDynamicStructuredBuffer(sizeof(Matrix4x4), kCapacity * kRegionCount);
    buffer_->SetName("bone matrix pool");

    //the identity block of every region is written once; it is what a mesh that
    //does not fit the pool falls back to, and it never changes
    std::vector<Matrix4x4> identity(kIdentityBones * 2, Matrix4x4::identity);
    for (uint32_t region = 0; region < kRegionCount; ++region) {
        size_t offset = region * kCapacity + kUsableCapacity;
        buffer_->Update(offset * sizeof(Matrix4x4), identity.data(), identity.size() * sizeof(Matrix4x4));
    }
}

void BoneMatrixPool::Release() {
    buffer_.reset();
    used_ = 0;
    region_ = 0;
}

uint32_t BoneMatrixPool::Allocate(uint32_t bone_count) {
    Setup();

    //the pose of the frame and the one before it
    uint32_t size = bone_count * 2;
    if (bone_count == 0 || used_ + size > kUsableCapacity) {
        LOG_ERR("bone matrix pool is full: {0} of {1} matrices are in use, a mesh of {2} bones is drawn in its bind pose",
            used_, kUsableCapacity, bone_count);
        return kInvalidBoneOffset;
    }

    uint32_t offset = used_;
    used_ += size;

    LOG_LOG("bone matrix pool: {0} of {1} matrices in use, {2} for a mesh of {3} bones",
        used_, kUsableCapacity, size, bone_count);

    return offset;
}

void BoneMatrixPool::Free(uint32_t offset, uint32_t bone_count) {
    if (offset == kInvalidBoneOffset) {
        return;
    }

    if (offset + bone_count * 2 == used_) {
        used_ = offset;
    }
}

void BoneMatrixPool::BeginFrame() {
    region_ = (region_ + 1) % kRegionCount;
}

void BoneMatrixPool::Update(uint32_t offset, const std::vector<Matrix4x4>& bones,
    const std::vector<Matrix4x4>& prev_bones)
{
    if (!buffer_ || offset == kInvalidBoneOffset || bones.empty()) {
        return;
    }

    assert(prev_bones.size() == bones.size());

    const size_t stride = sizeof(Matrix4x4);
    uint32_t frame_offset = FrameOffset(offset);

    buffer_->Update(frame_offset * stride, bones.data(), bones.size() * stride);
    buffer_->Update((frame_offset + bones.size()) * stride, prev_bones.data(), prev_bones.size() * stride);
}

}
}
