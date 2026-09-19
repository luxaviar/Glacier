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

    ResourceType resource_type() const { return resource_type_; }
    ResourceState& GetState() { return state_; }
    uint32_t GetSubresourceNum() const { return state_.num_subresources; }

    ResourceAccessBit GetResourceState(uint32_t subresource = BARRIER_ALL_SUBRESOURCES);
    void SetResourceState(ResourceAccessBit state, uint32_t subresource = BARRIER_ALL_SUBRESOURCES);
    bool IsUniformState();

    //A resource whose state cannot change: it lives in an upload heap, where
    //GENERIC_READ is the only state the runtime accepts, so a barrier on it is
    //an error instead of a transition.
    bool has_fixed_state() const { return fixed_state_; }
    void SetFixedState() { fixed_state_ = true; }

    virtual void* GetNativeResource() const { return nullptr; }

protected:
    ResourceType resource_type_ = ResourceType::kBuffer;
    bool fixed_state_ = false;
    std::string name_;
    ResourceState state_ = { 0, ResourceAccessBit::kCommon };
};

}
}
