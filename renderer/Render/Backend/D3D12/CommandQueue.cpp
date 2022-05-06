#include "CommandQueue.h"
//#include "CommandList.h"
#include "GfxDriver.h"
#include "Exception/Exception.h"
#include "Common/Util.h"
#include "Common/Log.h"

namespace glacier {
namespace render {

D3D12CommandQueue::D3D12CommandQueue(D3D12GfxDriver* driver, D3D12_COMMAND_LIST_TYPE type) :
    device_(driver->GetDevice()),
    type_(type)
{
    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type                     = type;
    desc.Priority                 = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask                 = 0;

    GfxThrowIfFailed(device_->CreateCommandQueue(&desc, IID_PPV_ARGS(&command_queue_)));
    GfxThrowIfFailed(device_->CreateFence(current_fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)));

    command_list_ = std::make_unique<D3D12CommandList>(device_, type_);
    pending_command_list_ = std::make_unique<D3D12CommandList>(device_, type_);

    switch (type) {
        case D3D12_COMMAND_LIST_TYPE_COPY:
        {
            command_queue_->SetName(TEXT("Copy Command Queue"));
            command_list_->SetName(TEXT("Copy Command List"));
            pending_command_list_->SetName(TEXT("Pending - Copy Command List"));
            break;
        }
        case D3D12_COMMAND_LIST_TYPE_COMPUTE:
        {
            command_queue_->SetName(TEXT("Compute Command Queue"));
            command_list_->SetName(TEXT("Compute Command List"));
            pending_command_list_->SetName(TEXT("Pending - Compute Command List"));
            break;
        }
        case D3D12_COMMAND_LIST_TYPE_DIRECT:
        {
            command_queue_->SetName(TEXT("Direct Command Queue"));
            command_list_->SetName(TEXT("Direct Command List"));
            pending_command_list_->SetName(TEXT("Pending - Direct Command List"));
            break;
        }
    }
}

HRESULT D3D12CommandQueue::SetName(const TCHAR* Name) {
    return command_queue_->SetName(Name);
}

D3D12CommandQueue::~D3D12CommandQueue() {
}

uint64_t D3D12CommandQueue::Signal() {
    // Advance the fence value to mark commands up to this fence point.
    uint64_t fenceValue = ++current_fence_value;
    // Add an instruction to the command queue to set a new fence point.  
    // Because we are on the GPU timeline, the new fence point won't be set until the GPU finishes
    // processing all the commands prior to this Signal().
    GfxThrowIfFailed(command_queue_->Signal(fence_.Get(), fenceValue));
    return fenceValue;
}

uint64_t D3D12CommandQueue::GetCompletedFenceValue() {
    return fence_->GetCompletedValue();
}

bool D3D12CommandQueue::IsFenceComplete(uint64_t fenceValue) {
    return fence_->GetCompletedValue() >= fenceValue;
}

void D3D12CommandQueue::WaitForFenceValue(uint64_t fenceValue) {
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

void D3D12CommandQueue::ResetCommandList() {
    command_list_->ResetAllocator();
    command_list_->Reset();

    if (pending_command_list_->IsClosed()) {
        pending_command_list_->ResetAllocator();
        pending_command_list_->Reset();
    }
}

// Returns the fence value to wait for for this command list.
void D3D12CommandQueue::ExecuteCommandList() {
    // Done recording commands.
    bool has_pending_barriers = command_list_->Close(pending_command_list_.get());
    if (has_pending_barriers) {
        pending_command_list_->Close();

        ID3D12CommandList* cmdsLists[] = { pending_command_list_->GetUnderlyingCommandList(),
            command_list_->GetUnderlyingCommandList() };
        command_queue_->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    }
    else {
        // Add the command list to the queue for execution.
        ID3D12CommandList* cmdsLists[] = { command_list_->GetUnderlyingCommandList() };
        command_queue_->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    }
}

void D3D12CommandQueue::Flush() {
    Signal();
    // Wait until the GPU has completed commands up to this fence point.
    WaitForFenceValue(current_fence_value);
}


}
}
