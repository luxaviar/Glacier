#include "CommandQueue.h"
#include "GfxDriver.h"
#include "Exception/Exception.h"
#include "Common/Util.h"
#include "Common/Log.h"
#include "CommandBuffer.h"
#include "Inspect/Profiler.h"

namespace glacier {
namespace render {

D3D12CommandQueue::D3D12CommandQueue(D3D12GfxDriver* driver, CommandBufferType type) :
    CommandQueue(driver, type)
{
    device_ = driver->GetDevice();
    native_type_ = GetNativeCommandBufferType(type);

    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = native_type_;
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;

    GfxThrowIfFailed(device_->CreateCommandQueue(&desc, IID_PPV_ARGS(&command_queue_)));
    GfxThrowIfFailed(device_->CreateFence(current_fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)));

    switch (native_type_) {
        case D3D12_COMMAND_LIST_TYPE_COPY:
        {
            command_queue_->SetName(TEXT("Copy Command Queue"));
            break;
        }
        case D3D12_COMMAND_LIST_TYPE_COMPUTE:
        {
            command_queue_->SetName(TEXT("Compute Command Queue"));
            break;
        }
        case D3D12_COMMAND_LIST_TYPE_DIRECT:
        {
            command_queue_->SetName(TEXT("Direct Command Queue"));
            break;
        }
        case D3D12_COMMAND_LIST_TYPE_BUNDLE:
        {
            command_queue_->SetName(TEXT("Bundle Command Queue"));
            break;
        }
    }
}

D3D12CommandQueue::~D3D12CommandQueue() {
}

void D3D12CommandQueue::SetName(const TCHAR* Name) {
    command_queue_->SetName(Name);
}

uint64_t D3D12CommandQueue::Signal() {
    uint64_t fenceValue = ++current_fence_value;
    GfxThrowIfFailed(command_queue_->Signal(fence_.Get(), fenceValue));
    return fenceValue;
}

void D3D12CommandQueue::Wait(const D3D12CommandQueue& other) {
    command_queue_->Wait(other.fence_.Get(), other.current_fence_value);
}

uint64_t D3D12CommandQueue::GetCompletedFenceValue() {
    return fence_->GetCompletedValue();
}

bool D3D12CommandQueue::IsFenceComplete(uint64_t fenceValue) {
    return fence_->GetCompletedValue() >= fenceValue;
}

void D3D12CommandQueue::WaitForFenceValue(uint64_t fenceValue) {
    PerfGuard gurad("WaitForFenceValue");
    if (!IsFenceComplete(fenceValue)) {
        auto event = ::CreateEvent(nullptr, false, false, nullptr);
        if (event) {
            // Fire event when GPU hits current fence.  
            fence_->SetEventOnCompletion(fenceValue, event);
            // Wait until the GPU hits current fence event is fired.
            ::WaitForSingleObject(event, DWORD_MAX);

            ::CloseHandle(event);
        }
    }
}

void D3D12CommandQueue::Flush() {
    Signal();
    // Wait until the GPU has completed commands up to this fence point.
    WaitForFenceValue(current_fence_value);

    ProccessInFlightCommandLists();
}

uint64_t D3D12CommandQueue::ExecuteCommandBuffer(std::vector<CommandBuffer*>& cmd_buffers) {
    //ResourceStateTracker::Lock();

    auto size = cmd_buffers.size();
    assert(size > 0);

    std::vector<D3D12CommandBuffer*> inflight_list;
    inflight_list.reserve(size * 2);  // 2x since each command list will have a pending command list.

    std::vector<CommandBuffer*> mipmap_list;
    mipmap_list.reserve(size);

    std::vector<ID3D12CommandList*> executed_list;
    executed_list.reserve(size * 2);

    for (size_t i = 0; i < size; ++i) {
        auto cmd_buffer = static_cast<D3D12CommandBuffer*>(cmd_buffers[i]);
        auto pending_cmd_buffer = GetNativeCommandBuffer();
        bool has_pending_barriers = cmd_buffer->Close(pending_cmd_buffer);

        if (has_pending_barriers) {
            pending_cmd_buffer->Close();
            executed_list.push_back(pending_cmd_buffer->GetNativeCommandList());
            inflight_list.push_back(pending_cmd_buffer);
        }
        else {
            available_command_lists_.Push(pending_cmd_buffer);
        }

        executed_list.push_back(cmd_buffer->GetNativeCommandList());
        inflight_list.push_back(cmd_buffer);

        auto mipmap_cmd_buffer = cmd_buffer->GetGenerateMipsCommandList();
        if (mipmap_cmd_buffer) {
            auto compute_buffer = static_cast<D3D12CommandBuffer*>(mipmap_cmd_buffer);
            mipmap_list.push_back(compute_buffer);
        }
    }

    UINT executed_num = static_cast<UINT>(executed_list.size());
    command_queue_->ExecuteCommandLists(executed_num, executed_list.data());
    uint64_t fence_value = Signal();

    //ResourceStateTracker::Unlock();

    // Queue command lists for reuse.
    for (auto commandList : inflight_list) {
        inflight_command_lists_.Push({ fence_value, commandList });
    }

    // If there are any command lists that generate mips then execute those
    // after the initial resource command lists have finished.
    if (mipmap_list.size() > 0) {
        auto compute_queue = static_cast<D3D12CommandQueue*>(driver_->GetCommandQueue(CommandBufferType::kCompute));
        compute_queue->Wait(*this);
        compute_queue->ExecuteCommandBuffer(mipmap_list);
    }

    return fence_value;
}

uint64_t D3D12CommandQueue::ExecuteCommandBuffer(CommandBuffer* cmd_buffer) {
    std::vector<CommandBuffer*> cmd_buffers{ cmd_buffer };
    return ExecuteCommandBuffer(cmd_buffers);
}

void D3D12CommandQueue::ProccessInFlightCommandLists() {
    CommandBufferEntry commandListEntry;

    while (inflight_command_lists_.TryPeek(commandListEntry)) {
        auto fenceValue = std::get<0>(commandListEntry);
        auto commandList = std::get<1>(commandListEntry);

        if (!IsFenceComplete(fenceValue)) {
            break;
        }

        inflight_command_lists_.Pop();

        commandList->Reset();

        available_command_lists_.Push(commandList);
    }
}

D3D12CommandBuffer* D3D12CommandQueue::GetNativeCommandBuffer() {
    auto cmd_buffer = GetCommandBuffer();
    return static_cast<D3D12CommandBuffer*>(cmd_buffer);
}

std::unique_ptr<CommandBuffer> D3D12CommandQueue::CreateCommandBuffer() {
    auto cmd_buffer = std::make_unique<D3D12CommandBuffer>(driver_, type_);
    switch (native_type_) {
        case D3D12_COMMAND_LIST_TYPE_COPY:
        {
            cmd_buffer->SetName("Copy Command List");
            break;
        }
        case D3D12_COMMAND_LIST_TYPE_COMPUTE:
        {
            cmd_buffer->SetName("Compute Command List");
            break;
        }
        case D3D12_COMMAND_LIST_TYPE_DIRECT:
        {
            cmd_buffer->SetName("Direct Command List");
            break;
        }
        case D3D12_COMMAND_LIST_TYPE_BUNDLE:
        {
            cmd_buffer->SetName("Bundle Command List");
            break;
        }
    }

    return cmd_buffer;
}

}
}
