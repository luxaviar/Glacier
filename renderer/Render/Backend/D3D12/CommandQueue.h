#pragma once

#include <d3d12.h>  // For ID3D12CommandQueue, ID3D12Device2, and ID3D12Fence
#include <atomic>              // For std::atomic_bool
#include <condition_variable>  // For std::condition_variable.
#include <cstdint>             // For uint64_t
#include "CommandList.h"

namespace glacier {
namespace render {

class D3D12GfxDriver;

class D3D12CommandQueue {
public:
    D3D12CommandQueue(D3D12GfxDriver* driver, D3D12_COMMAND_LIST_TYPE type);
    virtual ~D3D12CommandQueue();

    D3D12CommandList* GetCommandList() { return command_list_.get(); }
    ID3D12CommandQueue* GetUnderlyingCommandQueue() const { return command_queue_.Get(); }
    HRESULT SetName(const TCHAR* Name);

    D3D12_COMMAND_LIST_TYPE GetType() const { return type_; }
    ID3D12Device* GetDevice() const { return device_; }

    uint64_t Signal();

    uint64_t GetCompletedFenceValue();
    bool IsFenceComplete(uint64_t fenceValue );
    void WaitForFenceValue(uint64_t fenceValue);

    void ResetCommandList();
    void ExecuteCommandList();
    void Flush();

private:
    ID3D12Device* device_ = nullptr;
    D3D12_COMMAND_LIST_TYPE type_;

    ComPtr<ID3D12CommandQueue> command_queue_ = nullptr;
    std::unique_ptr<D3D12CommandList> command_list_;

    ComPtr<ID3D12Fence> fence_ = nullptr;
    UINT64 current_fence_value = 0;
};

}
}
