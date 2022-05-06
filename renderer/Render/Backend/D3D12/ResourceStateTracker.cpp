#include "ResourceStateTracker.h"
#include "Exception/Exception.h"
#include "GfxDriver.h"

namespace glacier {
namespace render {

void ResourceStateTracker::TransitionResource(D3D12Resource* resource,
    D3D12_RESOURCE_STATES state_after, UINT subresource)
{
    auto pResource = resource->GetUnderlyingResource().Get();

    // Create pending_barrier descriptor
    D3D12_RESOURCE_BARRIER barrier;
    ZeroMemory(&barrier, sizeof(D3D12_RESOURCE_BARRIER));

    // Describe pending_barrier
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = pResource;
    barrier.Transition.Subresource = subresource;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barrier.Transition.StateAfter = state_after;

    auto it = final_states_.find(resource);
    if (it != final_states_.end()) {
        auto& cur_state = it->second;
        if (subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES && !cur_state.subresource_states.empty()) {
            for (UINT i = 0; i < cur_state.subresource_states.size(); ++i)
            {
                auto subresource_state = cur_state.subresource_states[i];
                if (subresource_state != state_after)
                {
                    D3D12_RESOURCE_BARRIER newBarrier = barrier;
                    newBarrier.Transition.Subresource = i;
                    newBarrier.Transition.StateBefore = subresource_state;
                    resource_barriers_.push_back(newBarrier);
                }
            }
        }
        else {
            auto subresourc_state = cur_state.GetState(subresource);
            if (subresourc_state != state_after) {
                D3D12_RESOURCE_BARRIER newBarrier = barrier;
                newBarrier.Transition.StateBefore = subresourc_state;
                resource_barriers_.push_back(newBarrier);
            }
        }

        cur_state.SetState(state_after, subresource);

    } else {
        pending_barriers_.emplace_back(resource, barrier);

        it = final_states_.emplace_hint(it, resource, resource->CalculateNumSubresources());
        it->second.SetState(state_after, subresource);
    }
}

void ResourceStateTracker::UAVBarrier(ID3D12Resource* resource) {
    if (!resource) return;

    auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(resource);

    resource_barriers_.push_back(barrier);
}

void ResourceStateTracker::AliasBarrier(ID3D12Resource* resource_before, ID3D12Resource* resource_after) {
    auto barrier = CD3DX12_RESOURCE_BARRIER::Aliasing(resource_before, resource_after);

    resource_barriers_.push_back(barrier);
}

uint32_t ResourceStateTracker::FlushPendingResourceBarriers(D3D12CommandList* commandList) {
    if (pending_barriers_.empty()) {
        return 0;
    }

    BarrierList barriers;
    barriers.reserve(pending_barriers_.size());

    for (auto& entry : pending_barriers_) {
        auto& resource = entry.resource;
        auto& pending_barrier = entry.barrier;
        auto& pending_transition = pending_barrier.Transition;

        if (pending_transition.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES &&
            !resource->IsUniformState())
        {
            for (UINT i = 0; i < resource->GetSubresourceNum(); ++i)
            {
                auto subresource_state = resource->GetResourceState(i);// current_state.subresource_states[i];
                if (subresource_state != pending_transition.StateAfter)
                {
                    D3D12_RESOURCE_BARRIER newBarrier = pending_barrier;
                    newBarrier.Transition.Subresource = i;
                    newBarrier.Transition.StateBefore = subresource_state;
                    barriers.push_back(newBarrier);
                }
            }
        }
        else {
            auto subresource_state = resource->GetResourceState(pending_transition.Subresource);
            if (subresource_state != pending_transition.StateAfter) {
                pending_transition.StateBefore = subresource_state;
                barriers.push_back(pending_barrier);
            }
        }
    }

    UINT numBarriers = static_cast<UINT>(barriers.size());
    if (numBarriers > 0)
    {
        commandList->ResourceBarrier(numBarriers, barriers.data());
    }

    pending_barriers_.clear();

    return numBarriers;
}

void ResourceStateTracker::FlushResourceBarriers(D3D12CommandList* commandList) {
    UINT numBarriers = static_cast<UINT>(resource_barriers_.size());
    if (numBarriers > 0) {
        commandList->ResourceBarrier(numBarriers, resource_barriers_.data());
        resource_barriers_.clear();
    }
}

void ResourceStateTracker::CommitFinalStates() {
    for (auto& [resource, state] : final_states_) {
        if (state.subresource_states.empty()) {
            resource->SetResourceState(state.state);
        }
        else {
            for (uint32_t i = 0; i < state.subresource_states.size(); ++i) {
                resource->SetResourceState(state.subresource_states[i], i);
            }
        }
    }

    final_states_.clear();
}

void ResourceStateTracker::Reset() {
    pending_barriers_.clear();
    resource_barriers_.clear();
    final_states_.clear();
}

}
}
