#pragma once

#include <vector>
#include <string>
#include "Common/Uncopyable.h"
#include "Core/Identifiable.h"
#include "Enums.h"

namespace glacier {
namespace render {

struct ResourceState {
    ResourceState(uint32_t num_subresources, ResourceAccessBit state = ResourceAccessBit::kCommon) :
        state(state),
        num_subresources(num_subresources)
    {}

    ResourceAccessBit GetState(uint32_t subresource = BARRIER_ALL_SUBRESOURCES);
    void SetState(ResourceAccessBit new_state, uint32_t subresource = BARRIER_ALL_SUBRESOURCES);

    ResourceAccessBit state;
    uint32_t num_subresources;
    std::vector<ResourceAccessBit> subresource_states;
};

class Resource : private Uncopyable, public Identifiable<Resource> {
public:
    virtual ~Resource() = default;

    virtual void SetName(const char* name) { name_ = name; };
    virtual const std::string& GetName() const { return name_; };

    ResourceState& GetState() { return state_; }
    uint32_t GetSubresourceNum() const { return state_.num_subresources; }

    ResourceAccessBit GetResourceState(uint32_t subresource = BARRIER_ALL_SUBRESOURCES);
    void SetResourceState(ResourceAccessBit state, uint32_t subresource = BARRIER_ALL_SUBRESOURCES);
    bool IsUniformState();

    virtual void* GetNativeResource() const { return nullptr; }

protected:
    std::string name_;
    ResourceState state_ = { 0, ResourceAccessBit::kCommon };
};

}
}
