#pragma once

#include <d3d11_1.h>
#include <map>
#include "Render/Base/Sampler.h"
#include "Render/Base/SamplerState.h"

namespace glacier {
namespace render {

class D3D11Sampler : public Sampler {
public:
    static std::shared_ptr<D3D11Sampler> Create(const SamplerState& ss);
    static void Clear();

    D3D11Sampler(const SamplerState& ss);
    D3D11Sampler(const SamplerBuilder& builder);
        
    void Bind(ShaderType shader_type, uint16_t slot) override;
    void UnBind(ShaderType shader_type, uint16_t slot) override;

    void* underlying_resource() const override { return sampler_state_.Get(); }

private:
    ComPtr<ID3D11SamplerState> sampler_state_;

private:
    static std::map<uint32_t, std::shared_ptr<D3D11Sampler>> cache_;
};

}
}
