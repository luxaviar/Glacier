#pragma once

#include <cstdint>
#include <memory>
#include "Common/Singleton.h"
#include "Math/Mat4.h"
#include "Math/Vec4.h"

namespace glacier {
namespace render {

class Buffer;

//One instance of a batched draw: everything a draw needs of an object, which
//the per object constant buffer cannot carry because it only holds the object
//that was bound last. Must match InstanceData in Common/Instance.hlsli.
struct alignas(16) InstanceData {
    Matrix4x4 model;
    Matrix4x4 prev_model;
    Vec4f tex_tile_scale;
    //slot of the bones of this instance in the bone matrix pool, and the slot
    //of the pose it was drawn with in the previous frame
    uint32_t bone_offset;
    uint32_t prev_bone_offset;
    uint32_t padding[2];
};

//The records of the instances of every batch of a frame. A batch takes a region
//of the buffer from the end of it and reports where the region starts through
//_InstanceOffset of its per object data; the vertex shader adds SV_InstanceID,
//so one draw call can carry all of them.
class InstanceBuffer : public Singleton<InstanceBuffer> {
public:
    //instances of every batch of one frame, 4096 of them are 640KB
    static constexpr uint32_t kCapacity = 4096;
    //frames in flight the buffer is kept for, see GfxDriver::kBufferCount
    static constexpr uint32_t kRegionCount = 3;
    static constexpr uint32_t kInvalidOffset = 0xffffffff;

    void Setup();
    void Release();

    //Switches to the region of the next frame; the records of a frame cannot be
    //overwritten while the GPU still draws the batches that read them.
    void BeginFrame();

    //copies the records of one batch and returns where they are in the buffer,
    //kInvalidOffset when the frame has no room left
    uint32_t Upload(const InstanceData* instances, uint32_t count);

    const std::shared_ptr<Buffer>& buffer() const { return buffer_; }
    uint32_t used() const { return used_; }

private:
    //records of one region, 4096 of them are 640KB
    static constexpr uint32_t kRegionCapacity = kCapacity;

    std::shared_ptr<Buffer> buffer_;
    //records handed out in the region of this frame
    uint32_t used_ = 0;
    uint32_t region_ = 0;
};

}
}
