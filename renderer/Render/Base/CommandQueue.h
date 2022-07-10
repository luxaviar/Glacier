#pragma once

#include <vector>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include "Concurrent/ThreadSafeQueue.h"
#include "CommandBuffer.h"

namespace glacier {
namespace render {

class GfxDriver;

class CommandQueue {
public:
    using CommandBufferEntry = std::tuple<uint64_t, CommandBuffer*>;

    CommandQueue(GfxDriver* driver, CommandBufferType type);
    virtual ~CommandQueue() {}

    CommandBufferType GetType() const { return type_; }
    virtual void SetName(const TCHAR* Name) = 0;

    virtual uint64_t Signal() = 0;

    virtual uint64_t GetCompletedFenceValue() = 0;
    virtual bool IsFenceComplete(uint64_t fenceValue ) = 0;
    virtual void WaitForFenceValue(uint64_t fenceValue) = 0;

    virtual void Flush() = 0;

    virtual CommandBuffer* GetCommandBuffer();

    virtual uint64_t ExecuteCommandBuffer(CommandBuffer* cmd_buffer) = 0;
    virtual uint64_t ExecuteCommandBuffer(std::vector<CommandBuffer*>& cmd_buffers) = 0;
    //virtual void Wait(const CommandQueue& other) = 0;

protected:
    virtual std::unique_ptr<CommandBuffer> CreateCommandBuffer() = 0;

    GfxDriver* driver_ = nullptr;
    CommandBufferType type_;
    uint64_t current_fence_value = 0;

    concurrent::ThreadSafeQueue<std::unique_ptr<CommandBuffer>> command_list_pool_;

    concurrent::ThreadSafeQueue<CommandBufferEntry> inflight_command_lists_;
    concurrent::ThreadSafeQueue<CommandBuffer*> available_command_lists_;
};

}
}
