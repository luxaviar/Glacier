#pragma once

#include <d3d12.h>
#include <array>
#include <vector>
#include <unordered_map>
#include "Shader.h"
#include "d3dx12.h"
#include "Render/Base/Program.h"
#include "Render/Material.h"

namespace glacier {
namespace render {

class D3D12Buffer;
class D3D12StructuredBuffer;
class D3D12Texture;
struct MaterialProperty;
class D3D12RenderTarget;
class D3D12Sampler;
class D3D12CommandList;

class D3D12Program : public Program {
public:
    struct ParameterBinding {
        uint32_t root_slot;
        uint32_t bind_count = 0;
        std::vector<D3D12ShaderParameter> params;

        operator bool() const { return !params.empty(); }
        size_t size() const { return params.size(); }

        D3D12ShaderParameter& operator[](size_t index) { return params[index]; }
        const D3D12ShaderParameter& operator[](size_t index) const { return params[index]; }
    };

    D3D12Program(const char* name);
    ID3D12RootSignature* GetRootSignature();

    bool SetParameter(const char* name, D3D12ConstantBuffer* cbuffer);
    bool SetParameter(const char* name, D3D12StructuredBuffer* sbuffer);
    bool SetParameter(const char* name, D3D12Texture* tex);
    bool SetParameter(const char* name, D3D12Sampler* tex);
    bool SetParameter(const char* name, const D3D12DescriptorRange& descriptor, ShaderParameterCatetory category = ShaderParameterCatetory::kSRV);

    void Bind(GfxDriver* gfx, Material* mat) override;
    void UnBind(GfxDriver* gfx, Material* mat) override {}
    void ReBind(GfxDriver* gfx, Material* mat) override;

    void Bind(GfxDriver* gfx, MaterialTemplate* mat) override;
    void UnBind(GfxDriver* gfx, MaterialTemplate* mat) override {}

    void Bind(D3D12CommandList* cmd_list);
    
protected:
    void CreateRootSignature();

    void SetupShaderParameter(const std::shared_ptr<Shader>& shader) override;
    void AddParameter(ParameterBinding& list, const D3D12ShaderParameter& param);

    void BindProperty(GfxDriver* gfx, const MaterialProperty& prop);

    D3D12_SHADER_VISIBILITY GetShaderVisibility(ShaderType type);
    //std::vector<CD3DX12_STATIC_SAMPLER_DESC> CreateStaticSamplers() const;

    ComPtr<ID3D12RootSignature> root_signature_;

    ParameterBinding cbv_def_;
    ParameterBinding srv_def_;
    ParameterBinding uav_def_;
    ParameterBinding sampler_def_;
};

}
}
