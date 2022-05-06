#pragma once

#include <d3d12.h>
#include <string>
#include "Resource.h"

namespace glacier {
namespace render {

class D3D12CommandList;

class ResourceStateTracker {
public:
    struct PendingState {
        PendingState(D3D12Resource* res, D3D12_RESOURCE_BARRIER barrier) : resource(res), barrier(barrier) {}

        D3D12Resource* resource;
        D3D12_RESOURCE_BARRIER barrier;
    };

    using BarrierList = std::vector<D3D12_RESOURCE_BARRIER>;
    using ResourceStateDictionary = std::unordered_map<D3D12Resource*, ResourceState>;

    ResourceStateTracker() {}

    void TransitionResource(D3D12Resource* resource, D3D12_RESOURCE_STATES state_after,
        UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

    void UAVBarrier(ID3D12Resource* resource = nullptr);
    void AliasBarrier(ID3D12Resource* resource_before = nullptr, ID3D12Resource* resource_after = nullptr);

    uint32_t FlushPendingResourceBarriers(D3D12CommandList* commandList);
    void FlushResourceBarriers(D3D12CommandList* commandList);

    void CommitFinalStates();
    void Reset();

private:
    std::vector<PendingState> pending_barriers_;
    BarrierList resource_barriers_;

    ResourceStateDictionary final_states_;
};

}
}
