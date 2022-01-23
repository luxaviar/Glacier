#pragma once

#include <d3d11_1.h>
#include <map>
#include "Render/Base/Resource.h"
#include "Render/Base/SamplerState.h"

namespace glacier {
namespace render {

class GfxDriver;

class D3D11Sampler : public Sampler {
public:
    static std::shared_ptr<D3D11Sampler> Create(GfxDriver* gfx, const SamplerState& ss);
    static void Clear();

    D3D11Sampler(GfxDriver* gfx, const SamplerState& ss);

    void Bind(ShaderType shader_type, uint16_t slot);
    void UnBind(ShaderType shader_type, uint16_t slot);

    void* underlying_resource() const override { return sampler_state_.Get(); }

private:
    static std::map<uint32_t, std::shared_ptr<D3D11Sampler>> cache_;

    ComPtr<ID3D11SamplerState> sampler_state_;
};

}
}
