#pragma once

#include <d3d12.h>  // For ID3D12CommandQueue, ID3D12Device2, and ID3D12Fence
#include <atomic>              // For std::atomic_bool
#include <condition_variable>  // For std::condition_variable.
#include <cstdint>             // For uint64_t
#include <string>
#include "Concurrent/ThreadSafeQueue.h"
#include "Render/Base/CommandQueue.h"

namespace glacier {
namespace render {

class D3D12GfxDriver;
class CommandBuffer;
class D3D12CommandBuffer;

class D3D12CommandQueue : public CommandQueue {
public:
    D3D12CommandQueue(D3D12GfxDriver* driver, CommandBufferType type);
    virtual ~D3D12CommandQueue();

    ID3D12CommandQueue* GetNativeCommandQueue() const { return command_queue_.Get(); }
    void SetName(const TCHAR* Name) override;

    D3D12_COMMAND_LIST_TYPE GetNativeType() const { return native_type_; }
    ID3D12Device* GetDevice() const { return device_; }

    uint64_t Signal();

    uint64_t GetCompletedFenceValue();
    bool IsFenceComplete(uint64_t fenceValue );
    // The name is what the profile shows the wait as, and it is a static string:
    // a frame flush, a backbuffer a present is about to write, the wait of a
    // queue for another one. They are told apart in the profile tree, which is
    // how "the frame waits for its own GPU work" is read off a report.
    // timeout_ms exists for the waits that must not hang the app; a wait that
    // times out says so in the log and returns, it does not throw
    void WaitForFenceValue(uint64_t fenceValue, const char* label = "WaitForFenceValue",
        uint32_t timeout_ms = 0xFFFFFFFF);

    void Flush();

    uint64_t ExecuteCommandBuffer(std::vector<CommandBuffer*>& cmd_buffers) override;
    uint64_t ExecuteCommandBuffer(CommandBuffer* cmd_buffer) override;

    void Wait(const D3D12CommandQueue& other);

    // Returns the command lists whose fence completed to the pool. Call it once
    // per frame, before a frame allocates any: it is what makes a frame that does
    // not wait for the GPU safe, since a list is only reset once the GPU is done
    // with everything recorded into it (see also Flush, which does it after it
    // waited for the whole queue).
    void CollectCompletedCommandLists();

    D3D12CommandBuffer* GetNativeCommandBuffer();

private:
    std::unique_ptr<CommandBuffer> CreateCommandBuffer() override;

    ID3D12Device* device_ = nullptr;
    D3D12_COMMAND_LIST_TYPE native_type_;

    ComPtr<ID3D12CommandQueue> command_queue_ = nullptr;
    ComPtr<ID3D12Fence> fence_ = nullptr;
    // what WaitForFenceValue waits on, kept for the life of the queue
    HANDLE wait_event_ = nullptr;
    // how many command lists this queue ever created; one that keeps growing
    // means the pool is not being recycled (see CollectCompletedCommandLists)
    uint32_t created_command_lists_ = 0;
    // what the queue calls itself in a log line
    std::string queue_name_ = "command queue";
};

}
}
