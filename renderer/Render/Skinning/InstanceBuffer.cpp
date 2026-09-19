#include "InstanceBuffer.h"
#include "Render/Base/Buffer.h"
#include "Render/Base/GfxDriver.h"
#include "Common/Log.h"

namespace glacier {
namespace render {

static_assert(InstanceBuffer::kRegionCount == GfxDriver::kBufferCount,
    "the instance records of a frame have to stay valid until that frame was presented");

void InstanceBuffer::Setup() {
    if (buffer_) {
        return;
    }

    buffer_ = GfxDriver::Get()->CreateDynamicStructuredBuffer(sizeof(InstanceData), kRegionCapacity * kRegionCount);
    buffer_->SetName("instance buffer");
}

void InstanceBuffer::Release() {
    buffer_.reset();
    used_ = 0;
    region_ = 0;
}

void InstanceBuffer::BeginFrame() {
    region_ = (region_ + 1) % kRegionCount;
    used_ = 0;
}

uint32_t InstanceBuffer::Upload(const InstanceData* instances, uint32_t count) {
    Setup();

    if (count == 0 || used_ + count > kRegionCapacity) {
        LOG_ERR("instance buffer is full: {0} of {1} instances are in use, a batch of {2} is drawn one object at a time",
            used_, kRegionCapacity, count);
        return kInvalidOffset;
    }

    uint32_t offset = region_ * kRegionCapacity + used_;
    used_ += count;

    buffer_->Update(offset * sizeof(InstanceData), instances, count * sizeof(InstanceData));

    return offset;
}

}
}
