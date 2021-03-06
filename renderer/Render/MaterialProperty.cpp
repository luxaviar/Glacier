#include "MaterialProperty.h"
#include "Render/Base/GfxDriver.h"

namespace glacier {
namespace render {

MaterialProperty::MaterialProperty(const ShaderParameter* param, const std::shared_ptr<Texture>& tex, const Color& default_color) :
    shader_param(param),
    prop_type(MaterialPropertyType::kTexture),
    default_color(default_color),
    use_default(!tex),
    resource(tex)
{
}

MaterialProperty::MaterialProperty(const ShaderParameter* param, const std::shared_ptr<Buffer>& buf) :
    shader_param(param),
    resource(buf)
{
    auto ty = buf->type();
    switch (ty)
    {
    case BufferType::kConstantBuffer:
        prop_type = MaterialPropertyType::kConstantBuffer;
        break;
    case BufferType::kStructuredBuffer:
        prop_type = MaterialPropertyType::kStructuredBuffer;
        break;
    case BufferType::kRWStructuredBuffer:
        prop_type = MaterialPropertyType::kRWStructuredBuffer;
        break;
    default:
        throw std::exception{ "Bad BufferType to MaterialProperty." };
        break;
    }
}

MaterialProperty::MaterialProperty(const ShaderParameter* param, const Color& color) :
    shader_param(param),
    prop_type(MaterialPropertyType::kColor),
    color(color)
{
}

MaterialProperty::MaterialProperty(const ShaderParameter* param, const Vec4f& float4) :
    shader_param(param),
    prop_type(MaterialPropertyType::kFloat4),
    float4(float4)
{
}

MaterialProperty::MaterialProperty(const ShaderParameter* param, const Matrix4x4& matrix) :
    shader_param(param),
    prop_type(MaterialPropertyType::kMatrix),
    matrix(matrix)
{
}

MaterialProperty::MaterialProperty(const ShaderParameter* param, const SamplerState& ss) :
    shader_param(param),
    prop_type(MaterialPropertyType::kSampler),
    sampler_state(ss)
{

}

MaterialProperty::MaterialProperty(const MaterialProperty& other) noexcept :
    dirty(other.dirty),
    shader_param(other.shader_param),
    prop_type(other.prop_type),
    use_default(other.use_default),
    resource(other.resource)
{
    switch (prop_type)
    {
    case MaterialPropertyType::kTexture:
        default_color = other.default_color;
        break;
    case MaterialPropertyType::kColor:
        color = other.color;
        break;
    case MaterialPropertyType::kFloat4:
        float4 = other.float4;
        break;
    case MaterialPropertyType::kMatrix:
        matrix = other.matrix;
        break;
    case MaterialPropertyType::kSampler:
        sampler_state = other.sampler_state;
    default:
        break;
    }
}

MaterialProperty::MaterialProperty(MaterialProperty&& other) noexcept :
    dirty(other.dirty),
    shader_param(other.shader_param),
    prop_type(other.prop_type),
    use_default(other.use_default),
    resource(std::move(other.resource))
{
    switch (prop_type)
    {
    case MaterialPropertyType::kTexture:
        default_color = other.default_color;
        break;
    case MaterialPropertyType::kColor:
        color = other.color;
        break;
    case MaterialPropertyType::kFloat4:
        float4 = other.float4;
        break;
    case MaterialPropertyType::kMatrix:
        matrix = other.matrix;
        break;
    case MaterialPropertyType::kSampler:
        sampler_state = other.sampler_state;
    default:
        break;
    }
}

}
}
