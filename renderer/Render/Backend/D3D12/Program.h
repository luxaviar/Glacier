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
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> bindings;

        operator bool() const { return !params.empty(); }
        size_t size() const { return params.size(); }

        D3D12ShaderParameter& operator[](size_t index) { return params[index]; }
        const D3D12ShaderParameter& operator[](size_t index) const { return params[index]; }
    };

    D3D12Program(const char* name);
    ID3D12RootSignature* GetRootSignature();

    void SetParameter(const char* name, D3D12ConstantBuffer* cbuffer);
    void SetParameter(const char* name, D3D12StructuredBuffer* sbuffer);
    void SetParameter(const char* name, D3D12Texture* tex);
    void SetParameter(const char* name, D3D12Sampler* tex);
    void SetParameter(const char* name, const D3D12DescriptorRange& descriptor, ShaderParameterCatetory category = ShaderParameterCatetory::kSRV);

    void BindPSO(GfxDriver* gfx) override;

    void Bind(GfxDriver* gfx, Material* mat) override;
    void UnBind(GfxDriver* gfx, Material* mat) override {}
    void RefreshDynamicBuffer(GfxDriver* gfx) override;

    void Bind(D3D12CommandList* cmd_list);

    bool IsCompute() const { return is_compute_; }
    
protected:
    void CreateRootSignature();

    void SetupShaderParameter(const std::shared_ptr<Shader>& shader) override;
    void AddParameter(ParameterBinding& list, const D3D12ShaderParameter& param);

    void BindProperty(GfxDriver* gfx, D3D12CommandList* cmd_list, const MaterialProperty& prop);

    bool is_compute_ = false;
    bool dynamic_buffer_ = false;
    ComPtr<ID3D12RootSignature> root_signature_;

    ParameterBinding cbv_def_;
    ParameterBinding srv_def_;
    ParameterBinding uav_def_;
    ParameterBinding sampler_def_;
};

}
}
