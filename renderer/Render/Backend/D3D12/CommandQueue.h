#pragma once

#include <d3d12.h>  // For ID3D12CommandQueue, ID3D12Device2, and ID3D12Fence
#include <atomic>              // For std::atomic_bool
#include <condition_variable>  // For std::condition_variable.
#include <cstdint>             // For uint64_t
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
    void WaitForFenceValue(uint64_t fenceValue);

    void Flush();

    uint64_t ExecuteCommandBuffer(std::vector<CommandBuffer*>& cmd_buffers) override;
    uint64_t ExecuteCommandBuffer(CommandBuffer* cmd_buffer) override;

    void Wait(const D3D12CommandQueue& other);

    void ProccessInFlightCommandLists();

    D3D12CommandBuffer* GetNativeCommandBuffer();

private:
    std::unique_ptr<CommandBuffer> CreateCommandBuffer() override;

    ID3D12Device* device_ = nullptr;
    D3D12_COMMAND_LIST_TYPE native_type_;

    ComPtr<ID3D12CommandQueue> command_queue_ = nullptr;
    ComPtr<ID3D12Fence> fence_ = nullptr;
};

}
}
