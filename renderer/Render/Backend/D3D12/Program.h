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
    static constexpr uint32_t kMaxRootSlot = 32;

    struct RootConstant {
        uint32_t num_32bit;
        uint32_t bind_point;
        uint32_t register_space;
        ShaderType shader_type = ShaderType::kUnknown;
    };

    struct DescriptorTableParam {
        uint32_t root_index;
        uint32_t bind_count = 0;
        std::vector<D3D12ShaderParameter> params;

        operator bool() const { return !params.empty(); }
        size_t size() const { return params.size(); }

        D3D12ShaderParameter& operator[](size_t index) { return params[index]; }
        const D3D12ShaderParameter& operator[](size_t index) const { return params[index]; }
    };

    D3D12Program(const char* name);
    ID3D12RootSignature* GetRootSignature();

    void SetRootConstants(const char* name, uint32_t num_32bit);
    void SetStaticSampler(const char* name, D3D12_STATIC_SAMPLER_DESC desc);

    void SetParameter(const std::string& name, D3D12ConstantBuffer* cbuffer);
    void SetParameter(const std::string& name, D3D12StructuredBuffer* sbuffer);
    void SetParameter(const std::string& name, D3D12Texture* tex);
    void SetParameter(const std::string& name, D3D12Sampler* tex);

    void BindPSO(GfxDriver* gfx) override;
    void Bind(GfxDriver* gfx, Material* mat) override;

    void RefreshTranstientBuffer(GfxDriver* gfx) override;

    const ComPtr<ID3D12RootSignature>& GetRootSignature() const { return root_signature_; }
    bool IsCompute() const { return is_compute_; }

    uint32_t GetDescriptorTableBitMask(D3D12_DESCRIPTOR_HEAP_TYPE heap_type) const;
    uint32_t GetNumDescriptors(uint32_t root_index) const;
    uint32_t GetNumParameters() const { return num_parameter_; }
    
protected:
    void CreateRootSignature();
    void Bind(D3D12CommandList* cmd_list);

    void SetupShaderParameter(const std::shared_ptr<Shader>& shader) override;
    void AddParameter(DescriptorTableParam& list, const D3D12ShaderParameter& param);

    void BindProperty(GfxDriver* gfx, D3D12CommandList* cmd_list, const MaterialProperty& prop);

    bool is_compute_ = false;
    ComPtr<ID3D12RootSignature> root_signature_;

    DescriptorTableParam cbv_table_;
    DescriptorTableParam srv_table_;
    DescriptorTableParam uav_table_;
    DescriptorTableParam sampler_table_;

    std::vector<RootConstant> root_constants_;
    std::vector<D3D12_STATIC_SAMPLER_DESC> static_smaplers_;
    std::vector<uint32_t> transtient_buffers_;

    uint32_t num_parameter_ = 0;
    std::array<uint32_t, kMaxRootSlot> num_descriptor_per_table_;
    uint32_t sampler_table_bitmask_ = 0;
    uint32_t srv_uav_table_bitmask_ = 0;
};

}
}
