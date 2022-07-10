#pragma once

#include "Enums.h"

namespace glacier {
namespace render {

class Resource;

struct BarrierDesc {
    BarrierType type;
    ResourceAccessBit before;
    ResourceAccessBit after;
    Resource* resource = nullptr;
    uint32_t subresource = 0;
};

}
}
