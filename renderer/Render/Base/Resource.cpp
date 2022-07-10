#pragma once

#include "Resource.h"

namespace glacier {
namespace render {

ResourceAccessBit ResourceState::GetState(uint32_t subresource) {
    if (subresource == BARRIER_ALL_SUBRESOURCES || subresource_states.empty())
    {
        return state;
    }
    else
    {
        if (subresource_states.empty()) {
            subresource_states.resize(num_subresources, state);
        }

        return subresource_states[subresource];
    }
}

void ResourceState::SetState(ResourceAccessBit new_state, uint32_t subresource) {
    if (subresource == BARRIER_ALL_SUBRESOURCES)
    {
        state = new_state;
        subresource_states.clear();
    }
    else
    {
        if (subresource_states.empty()) {
            subresource_states.resize(num_subresources, state);
        }

        subresource_states[subresource] = new_state;
    }
}

ResourceAccessBit Resource::GetResourceState(uint32_t subresource) {
    return state_.GetState(subresource);
}

void Resource::SetResourceState(ResourceAccessBit state, uint32_t subresource) {
    state_.SetState(state, subresource);
}

bool Resource::IsUniformState() {
    return state_.subresource_states.empty();
}

}
}
