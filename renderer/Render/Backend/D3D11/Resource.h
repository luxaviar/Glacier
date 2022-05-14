#pragma once

#include <d3d11_1.h>

namespace glacier {
namespace render {

class D3D11Resource {
public:
    virtual ID3D11Resource* GetUnderlyingResource() const = 0;
};

}
}
