#include "Program.h"
#include <d3d12shader.h>
#include <d3dcompiler.h>
#include "Common/TypeTraits.h"
#include "exception/exception.h"
#include "GfxDriver.h"
#include "Buffer.h"
#include "Texture.h"
#include "Render/Material.h"
#include "RenderTarget.h"
#include "PipelineState.h"
#include "Sampler.h"
#include "Inspect/Profiler.h"
#include "Common/Log.h"
#include "CommandBuffer.h"
#include "Buffer.h"

namespace glacier {
namespace render {

D3D12Program::D3D12Program(const char* name) : Program(name) {

}

ID3D12RootSignature* D3D12Program::GetRootSignature() {
    if (!root_signature_) {
        CreateRootSignature();
    }

    return root_signature_.Get();
}

uint32_t D3D12Program::GetDescriptorTableBitMask(D3D12_DESCRIPTOR_HEAP_TYPE heap_type) const {
    if (heap_type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER) {
        return sampler_table_bitmask_;
    }

    if (heap_type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) {
        return srv_uav_table_bitmask_;
    }

    return 0;
}

uint32_t D3D12Program::GetNumDescriptors(uint32_t root_index) const {
    return num_descriptor_per_table_[root_index];
}

void D3D12Program::SetupShaderParameter(const std::shared_ptr<Shader>& shader) {
    ID3DBlob* blob = shader->GetBytecode();
    ShaderType type = shader->type();

    is_compute_ = type == ShaderType::kCompute;

    ID3D12ShaderReflection* reflection = nullptr;
    D3DReflect(blob->GetBufferPointer(), blob->GetBufferSize(), IID_ID3D12ShaderReflection, (void**)&reflection);

    D3D12_SHADER_DESC shader_desc;
    reflection->GetDesc(&shader_desc);

    for (UINT i = 0; i < shader_desc.BoundResources; i++) {
        D3D12_SHADER_INPUT_BIND_DESC resource_desc;
        reflection->GetResourceBindingDesc(i, &resource_desc);

        auto param_name = resource_desc.Name;
        auto resource_type = resource_desc.Type;
        auto register_space = resource_desc.Space;
        auto bind_point = resource_desc.BindPoint;
        auto bind_count = resource_desc.BindCount;

        ShaderParameter param;
        param.type = GetShaderParameterType(resource_type);
        switch (resource_type) 
        {
        case D3D_SHADER_INPUT_TYPE::D3D_SIT_CBUFFER:
            param.name = param_name;
            param.shader_type = type;
            param.bind_point = bind_point;
            param.register_space = register_space;
            param.category = ShaderParameterCategory::kCBV;
            AddParameter(cbv_table_, param);
            break;
        case D3D_SHADER_INPUT_TYPE::D3D_SIT_TEXTURE:
        case D3D_SHADER_INPUT_TYPE::D3D_SIT_STRUCTURED:
        case D3D_SHADER_INPUT_TYPE::D3D_SIT_BYTEADDRESS:
            param.name = param_name;
            param.shader_type = type;
            param.bind_point = bind_point;
            param.bind_count = bind_count;
            param.register_space = register_space;
            param.category = ShaderParameterCategory::kSRV;
            AddParameter(srv_table_, param);
            break;
        case D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_RWTYPED:
        case D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_RWSTRUCTURED:
        case D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_RWBYTEADDRESS:
            assert(type == ShaderType::kCompute);
            param.name = param_name;
            param.shader_type = type;
            param.bind_point = bind_point;
            param.bind_count = bind_count;
            param.register_space = register_space;
            param.category = ShaderParameterCategory::kUAV;
            AddParameter(uav_table_, param);
            break;
        case D3D_SHADER_INPUT_TYPE::D3D_SIT_SAMPLER:
            assert(type == ShaderType::kPixel || type == ShaderType::kCompute);
            param.name = param_name;
            param.shader_type = type;
            param.bind_point = bind_point;
            param.register_space = register_space;
            param.category = ShaderParameterCategory::kSampler;
            AddParameter(sampler_table_, param);
            break;
        }

        SetShaderParameter(param.name, param);
    }

    //TODO: rearrage me
    if (FindParameter("linear_sampler") && !HasProperty("linear_sampler")) {
        SamplerState linear_ss;
        linear_ss.filter = FilterMode::kBilinear;
        linear_ss.warpU = linear_ss.warpV = linear_ss.warpW  = WarpMode::kClamp;
        SetProperty("linear_sampler", linear_ss);
    }

    if (FindParameter("_linear_clamp_sampler") && !HasProperty("_linear_clamp_sampler")) {
        SamplerState linear_ss;
        linear_ss.filter = FilterMode::kBilinear;
        linear_ss.warpU = linear_ss.warpV = linear_ss.warpW = WarpMode::kClamp;
        SetProperty("_linear_clamp_sampler", linear_ss);
    }

    if (FindParameter("_point_clamp_sampler") && !HasProperty("_point_clamp_sampler")) {
        SamplerState point_ss;
        point_ss.filter = FilterMode::kPoint;
        point_ss.warpU = point_ss.warpV = point_ss.warpW  = WarpMode::kClamp;
        SetProperty("_point_clamp_sampler", point_ss);
    }
}

void D3D12Program::AddParameter(DescriptorTableParam& table, const ShaderParameter& param) {
    for (const auto& p : table.params) {
        assert(p.name != param.name || p.shader_type != param.shader_type);
    }

    table.params.push_back(param);
    table.bind_count += param.bind_count;
}

void D3D12Program::SetRootConstants(const char* name, uint32_t num_32bit) {
    auto& params = cbv_table_.params;
    for (auto it = params.begin(); it != params.end(); ++it) {
        auto& param = *it;
        if (param.name == name) {
            root_constants_.push_back({ num_32bit, param.bind_point, param.register_space, param.shader_type });

            params.erase(it);
            break;
        }
    }
}

void D3D12Program::SetStaticSampler(const char* name, D3D12_STATIC_SAMPLER_DESC desc) {
    auto& params = sampler_table_.params;
    for (auto it = params.begin(); it != params.end(); ++it) {
        auto& param = *it;
        if (param.name == name) {
            assert(param.bind_point == desc.ShaderRegister);
            static_smaplers_.push_back(desc);

            params.erase(it);
            break;
        }
    }
}

void D3D12Program::BindPSO(CommandBuffer* cmd_buffer) {
    if (pso_) {
        auto d3d12pso = static_cast<D3D12PipelineState*>(pso_.get());
        d3d12pso->Bind(cmd_buffer);
    }
}

void D3D12Program::RefreshTranstientBuffer(CommandBuffer* cmd_buffer) {
    if (transtient_buffers_.empty()) return;

    for (auto i : transtient_buffers_) {
        auto& param = cbv_table_[i];
        assert(param.resource);
        cmd_buffer->SetConstantBufferView(cbv_table_.root_index + i, param.resource);
    }
}

ResourceAccessBit D3D12Program::GetShaderResourceTargetState(bool uav) {
    if (uav) {
        assert(is_compute_);
        return ResourceAccessBit::kShaderWrite;
    }
    else {
        return is_compute_ ? ResourceAccessBit::kNonPixelShaderRead : ResourceAccessBit::kPixelShaderRead;
    }
}

void D3D12Program::BindParameter(D3D12CommandBuffer* cmd_buffer, const std::string& name, D3D12ConstantBuffer* cbuffer) {
    auto& params = cbv_table_.params;
    for (uint32_t i = 0; i < params.size(); ++i) {
        auto& param = params[i];
        if (param.name == name) {
            if (param.resource != cbuffer) {
                param.resource = cbuffer;
                auto it = std::find(transtient_buffers_.begin(), transtient_buffers_.end(), i);
                if (cbuffer->IsDynamic()) {
                    if (it == transtient_buffers_.end()) {
                        transtient_buffers_.push_back(i);
                    }
                }
                else {
                    if (it != transtient_buffers_.end()) {
                        transtient_buffers_.erase(it);
                    }
                }
            }

            cmd_buffer->SetConstantBufferView(cbv_table_.root_index + i, cbuffer);
            return;
        }
    }
}

void D3D12Program::BindParameter(D3D12CommandBuffer* cmd_buffer, const std::string& name, D3D12Buffer* buffer, bool uav) {
    DescriptorTableParam& table = uav ? uav_table_ : srv_table_;
    auto& params = table.params;
    for (uint32_t i = 0; i < params.size(); ++i) {
        auto& param = params[i];
        if (param.name == name) {
            auto target_state = GetShaderResourceTargetState(uav);
            cmd_buffer->TransitionBarrier(buffer, target_state);
            cmd_buffer->SetDescriptorTable(table.root_index, i, buffer, uav);
            return;
        }
    }
}

void D3D12Program::BindParameter(D3D12CommandBuffer* cmd_buffer, const std::string& name, D3D12Texture* tex, bool uav) {
    DescriptorTableParam& table = uav ? uav_table_ : srv_table_;
    auto& params = table.params;
    for (uint32_t i = 0; i < params.size(); ++i) {
        auto& param = params[i];
        if (param.name == name) {
            auto target_state = GetShaderResourceTargetState(uav);
            cmd_buffer->TransitionBarrier(tex, target_state);

            cmd_buffer->SetDescriptorTable(table.root_index, i, tex, uav);
            return;
        }
    }
}

void D3D12Program::BindParameter(D3D12CommandBuffer* cmd_buffer, const std::string& name, D3D12Sampler* sampler) {
    auto& params = sampler_table_.params;
    for (uint32_t i = 0; i < params.size(); ++i) {
        auto& param = params[i];
        if (param.name == name) {
            cmd_buffer->SetSamplerTable(sampler_table_.root_index, i, sampler);
            return;
        }
    }
}

void D3D12Program::Bind(CommandBuffer* cmd_buffer, Material* mat) {
    auto cmd_list = static_cast<D3D12CommandBuffer*>(cmd_buffer);

    auto& properties = mat->GetProperties();
    for (auto& [_, prop] : properties) {
        BindProperty(cmd_list, prop);
    }
}

void D3D12Program::BindProperty(D3D12CommandBuffer* cmd_buffer, const MaterialProperty& prop) {
    auto param = prop.shader_param;
    auto& name = param->name;
    switch (param->type)
    {
    case ShaderParameterType::kTexture:
    {
        if ((prop.use_default && prop.dirty) || !prop.resource) {
            prop.resource = cmd_buffer->CreateTextureFromColor(prop.color, false);
            prop.resource->SetName(name.c_str());
        }

        auto tex = static_cast<D3D12Texture*>(prop.resource.get());
        BindParameter(cmd_buffer, name, tex, prop.shader_param->category == ShaderParameterCategory::kUAV);
    }
    break;
    case ShaderParameterType::kSampler:
    {
        if (!prop.resource || prop.dirty) {
            auto gfx = cmd_buffer->GetDriver();
            prop.resource = D3D12Sampler::Create(gfx, prop.sampler_state);
        }

        auto sampler = static_cast<D3D12Sampler*>(prop.resource.get());
        BindParameter(cmd_buffer, name, sampler);
    }
    break;
    case ShaderParameterType::kConstantBuffer:
    {
        if (!prop.resource) {
            auto gfx = cmd_buffer->GetDriver();
            if (prop.prop_type == ConstantPropertyType::kColor) {
                prop.resource = gfx->CreateConstantBuffer<Color>(prop.color, UsageType::kDefault);
            }
            else if (prop.prop_type == ConstantPropertyType::kFloat4) {
                prop.resource = gfx->CreateConstantBuffer<Vec4f>(prop.float4, UsageType::kDefault);
            }
            else {
                prop.resource = gfx->CreateConstantBuffer<Matrix4x4>(prop.matrix, UsageType::kDefault);
            }
        }
        else if (prop.dirty) {
            static_cast<Buffer*>(prop.resource.get())->Update(&prop.matrix); //also works for color & float4
        }

        auto cbuffer = static_cast<D3D12ConstantBuffer*>(prop.resource.get());
        BindParameter(cmd_buffer, name, cbuffer);
    }
    break;
    case ShaderParameterType::kStructuredBuffer:
    case ShaderParameterType::kByteAddressBuffer:
    {
        auto buf = static_cast<D3D12Buffer*>(prop.resource.get());
        BindParameter(cmd_buffer, name, buf);
    }
    break;
    case ShaderParameterType::kRWStructuredBuffer:
    case ShaderParameterType::kRWByteAddressBuffer:
    case ShaderParameterType::kRWTyped:
    {
        auto buf = static_cast<D3D12Buffer*>(prop.resource.get());
        BindParameter(cmd_buffer, name, buf, true);
    }
    break;
    default:
        throw std::exception{ "Bad parameter type for D3D12Program::Bind." };
        break;
    }
    prop.dirty = false;
}

void D3D12Program::Bind(D3D12CommandBuffer* cmd_list) {
    for (uint32_t i = 0; i < cbv_table_.size(); ++i) {
        const auto& param = cbv_table_[i];
        assert(param.resource);

        cmd_list->SetConstantBufferView(cbv_table_.root_index + i, param.resource);
    }

    for (uint32_t i = 0; i < srv_table_.params.size(); ++i) {
        auto& param = srv_table_.params[i];
        //auto target_state = GetTargetState(false);
        //cmd_list->TransitionBarrier(param.resource, target_state);
        cmd_list->SetDescriptorTable(srv_table_.root_index, i, param.resource);
    }

    for (uint32_t i = 0; i < uav_table_.params.size(); ++i) {
        auto& param = uav_table_.params[i];
        //auto target_state = GetTargetState(true);
        //cmd_list->TransitionBarrier(param.resource, target_state);
        cmd_list->SetDescriptorTable(uav_table_.root_index, i, param.resource, true);
    }

    for (uint32_t i = 0; i < sampler_table_.params.size(); ++i) {
        auto& param = sampler_table_.params[i];
        cmd_list->SetSamplerTable(sampler_table_.root_index, i, param.resource);
    }
}

void D3D12Program::CreateRootSignature() {
    std::vector<CD3DX12_ROOT_PARAMETER> root_params;

    // CBV
    //root constant
    for (auto& param : root_constants_) {
        CD3DX12_ROOT_PARAMETER RootParam;
        RootParam.InitAsConstants(param.num_32bit, param.bind_point, param.register_space, GetShaderVisibility(param.shader_type));
        root_params.push_back(RootParam);
    }

    //constant buffer view
    if (cbv_table_) {
        cbv_table_.root_index = (uint32_t)root_params.size();
        for (auto& param : cbv_table_.params)
        {
            CD3DX12_ROOT_PARAMETER RootParam;
            RootParam.InitAsConstantBufferView(param.bind_point, param.register_space, GetShaderVisibility(param.shader_type));
            root_params.push_back(RootParam);
        }
    }

    // SRV - descriptor table
    std::vector<CD3DX12_DESCRIPTOR_RANGE> srv_table;
    if (srv_table_) {
        auto root_index = (uint32_t)root_params.size();
        srv_table_.root_index = root_index;

        uint32_t num_descriptor = 0;
        srv_table.resize(srv_table_.params.size());
        for (size_t i = 0; i < srv_table_.params.size(); ++i) {
            auto& param = srv_table_.params[i];
            srv_table[i].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, param.bind_count, param.bind_point, param.register_space);
            num_descriptor += param.bind_count;
        }

        CD3DX12_ROOT_PARAMETER RootParam;
        D3D12_SHADER_VISIBILITY ShaderVisibility = is_compute_ ? D3D12_SHADER_VISIBILITY_ALL : D3D12_SHADER_VISIBILITY_PIXEL;
        RootParam.InitAsDescriptorTable((uint32_t)srv_table.size(), srv_table.data(), ShaderVisibility);
        root_params.push_back(RootParam);

        num_descriptor_per_table_[root_index] = num_descriptor;
        srv_uav_table_bitmask_ |= (1 << root_index);
    }

    // UAV - descriptor table
    std::vector<CD3DX12_DESCRIPTOR_RANGE> uav_table;
    if (uav_table_) {
        auto root_index = (uint32_t)root_params.size();
        uav_table_.root_index = root_index;

        uint32_t num_descriptor = 0;
        uav_table.resize(uav_table_.params.size());
        for (size_t i = 0; i < uav_table_.params.size(); ++i) {
            auto& param = uav_table_.params[i];
            uav_table[i].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, param.bind_count, param.bind_point, param.register_space);
            num_descriptor += param.bind_count;
        }

        CD3DX12_ROOT_PARAMETER RootParam;
        D3D12_SHADER_VISIBILITY ShaderVisibility = is_compute_ ? D3D12_SHADER_VISIBILITY_ALL : D3D12_SHADER_VISIBILITY_PIXEL;
        RootParam.InitAsDescriptorTable((uint32_t)uav_table.size(), uav_table.data(), ShaderVisibility);
        root_params.push_back(RootParam);

        num_descriptor_per_table_[root_index] = num_descriptor;
        srv_uav_table_bitmask_ |= (1 << root_index);
    }

    // Sampler - descriptor table
    std::vector<CD3DX12_DESCRIPTOR_RANGE> sampler_table;
    if (sampler_table_) {
        auto root_index = (uint32_t)root_params.size();
        sampler_table_.root_index = root_index;

        uint32_t num_descriptor = 0;
        sampler_table.resize(sampler_table_.params.size());
        for (size_t i = 0; i < sampler_table_.params.size(); ++i) {
            auto& param = sampler_table_.params[i];
            sampler_table[i].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, param.bind_count, param.bind_point, param.register_space);
            num_descriptor += param.bind_count;
        }

        CD3DX12_ROOT_PARAMETER RootParam;
        D3D12_SHADER_VISIBILITY ShaderVisibility = is_compute_ ? D3D12_SHADER_VISIBILITY_ALL : D3D12_SHADER_VISIBILITY_PIXEL;
        RootParam.InitAsDescriptorTable((uint32_t)sampler_table.size(), sampler_table.data(), ShaderVisibility);
        root_params.push_back(RootParam);

        num_descriptor_per_table_[root_index] = num_descriptor;
        sampler_table_bitmask_ |= (1 << root_index);
    }

    num_parameter_ = root_params.size();

    CD3DX12_ROOT_SIGNATURE_DESC sig_desc((UINT)root_params.size(), root_params.data(),
        static_smaplers_.size(), static_smaplers_.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> sig_blob = nullptr;
    ComPtr<ID3DBlob> err_blob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&sig_desc, D3D_ROOT_SIGNATURE_VERSION_1,
        sig_blob.GetAddressOf(), err_blob.GetAddressOf());

    if (err_blob != nullptr) {
        LOG_LOG("{}", (char*)err_blob->GetBufferPointer());
        err_blob->Release();
    }

    GfxThrowCheck(hr);

    GfxThrowIfFailed(D3D12GfxDriver::Instance()->GetDevice()->CreateRootSignature(
        0,
        sig_blob->GetBufferPointer(),
        sig_blob->GetBufferSize(),
        IID_PPV_ARGS(&root_signature_)));
}

}
}
