#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include "Common/Singleton.h"
#include "Math/Mat4.h"

namespace glacier {
namespace render {

class Buffer;

//reported by a draw that is not skinned at all
constexpr uint32_t kInvalidBoneOffset = 0xffffffff;

//The skinning matrices of a frame live in one structured buffer shared by every
//skinned mesh instead of in a constant buffer per object: a constant buffer has
//to be sized for the worst case a shader accepts (128 bones, see kMaxBones)
//while a slot only holds the bones the mesh really has, and an instanced draw
//can index the slot of every instance of the draw.
//
//The buffer keeps one region per frame in flight, so the matrices a frame is
//drawn with cannot be overwritten by the frame after it while the GPU still
//reads them. A slot has the same offset in every region, so a draw only has to
//add the region of the current frame (FrameOffset) to the offset of its slot.
class BoneMatrixPool : public Singleton<BoneMatrixPool> {
public:
    //matrices per region, 32768 of them are 2MB; the three regions of the pool
    //are 6MB of the memory of the GPU, which is room for 250 characters of the
    //65 bone rig of the demo (every one of them takes 2 x 65 matrices, one slot
    //per pose of the frame and one for the pose before it)
    static constexpr uint32_t kCapacity = 32768;
    //frames in flight the buffer is split for, see GfxDriver::kBufferCount
    static constexpr uint32_t kRegionCount = 3;
    //bones past kMaxBones are dropped at import, so this much room at the end of
    //every region always holds the identity and is what a mesh that does not fit
    //the pool is drawn with: it keeps its bind pose instead of collapsing
    static constexpr uint32_t kIdentityBones = 128;

    void Setup();
    void Release();

    //reserves room for the current and the previous pose of one mesh; the
    //offset is kInvalidBoneOffset when the pool is full
    uint32_t Allocate(uint32_t bone_count);
    //A slot cannot be handed out again, only the slot at the end of the pool
    //shrinks it, which is enough because meshes keep their slot until they die.
    void Free(uint32_t offset, uint32_t bone_count);

    //switches to the region of the next frame; call it once per frame, before
    //anything uploads or draws
    void BeginFrame();

    //copies the pose of a mesh into its slot of the current region
    void Update(uint32_t offset, const std::vector<Matrix4x4>& bones,
        const std::vector<Matrix4x4>& prev_bones);

    //offset of a slot in the region of the current frame
    uint32_t FrameOffset(uint32_t offset) const { return region_ * kCapacity + offset; }
    //the previous pose of a mesh of `bone_count` bones sits behind its pose
    uint32_t PrevFrameOffset(uint32_t offset, uint32_t bone_count) const { return FrameOffset(offset) + bone_count; }
    //identity matrices a mesh without a slot is drawn with
    uint32_t IdentityBoneOffset() const { return FrameOffset(kCapacity - kIdentityBones * 2); }

    const std::shared_ptr<Buffer>& buffer() const { return buffer_; }

    uint32_t used() const { return used_; }
    uint32_t region() const { return region_; }

private:
    //matrices a mesh that does not fit the pool may use
    static constexpr uint32_t kUsableCapacity = kCapacity - kIdentityBones * 2;

    std::shared_ptr<Buffer> buffer_;
    //matrices of a region handed out so far
    uint32_t used_ = 0;
    uint32_t region_ = 0;
};

}
}
