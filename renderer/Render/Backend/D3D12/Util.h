#pragma once

#include <d3d12.h>
#include "Render/Base/Enums.h"
#include "Render/Base/Util.h"

namespace glacier {
namespace render {

// Custom resource states
constexpr D3D12_RESOURCE_STATES D3D12_RESOURCE_STATE_UNKNOWN = static_cast<D3D12_RESOURCE_STATES>(-1);
//constexpr D3D12_RESOURCE_STATES D3D12_RESOURCE_STATE_UNINITIALIZED = static_cast<D3D12_RESOURCE_STATES>(-2);

inline D3D12_CULL_MODE GetUnderlyingCullMode(CullingMode mode) {
    switch (mode)
    {
    case CullingMode::kNone:
        return D3D12_CULL_MODE_NONE;
    case CullingMode::kFront:
        return D3D12_CULL_MODE_FRONT;
    case CullingMode::kBack:
        return D3D12_CULL_MODE_BACK;
    default:
        throw std::exception{ "Bad CullingMode to D3D12_CULL_MODE." };
    }
}

inline D3D12_COMPARISON_FUNC GetUnderlyingCompareFunc(CompareFunc cmp) {
    switch (cmp)
    {
    case CompareFunc::kNever:
        return D3D12_COMPARISON_FUNC_NEVER;
    case CompareFunc::kLess:
        return D3D12_COMPARISON_FUNC_LESS;
    case CompareFunc::kEqual:
        return D3D12_COMPARISON_FUNC_EQUAL;
    case CompareFunc::kLessEqual:
        return D3D12_COMPARISON_FUNC_LESS_EQUAL;
    case CompareFunc::kGreater:
        return D3D12_COMPARISON_FUNC_GREATER;
    case CompareFunc::kNotEqual:
        return D3D12_COMPARISON_FUNC_NOT_EQUAL;
    case CompareFunc::kGreaterEqual:
        return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    case CompareFunc::kAlways:
        return D3D12_COMPARISON_FUNC_ALWAYS;
    default:
        throw std::exception{ "Bad Sampler CmpFunc to D3D12_COMPARISON_FUNC." };
    }
}

inline D3D12_STENCIL_OP GetUnderlyingStencilOP(StencilOp op) {
    switch (op)
    {
    case StencilOp::kKeep:
        return D3D12_STENCIL_OP_KEEP;
    case StencilOp::kZero:
        return D3D12_STENCIL_OP_ZERO;
    case StencilOp::kReplace:
        return D3D12_STENCIL_OP_REPLACE;
    case StencilOp::kIncrease:
        return D3D12_STENCIL_OP_INCR;
    case StencilOp::kIncreaseSaturate:
        return D3D12_STENCIL_OP_INCR_SAT;
    case StencilOp::kDecrease:
        return D3D12_STENCIL_OP_DECR;
    case StencilOp::kDecreaseSaturate:
        return D3D12_STENCIL_OP_DECR_SAT;
    case StencilOp::kInvert:
        return D3D12_STENCIL_OP_INVERT;
    default:
        throw std::exception{ "Bad StencilOp to D3D12_STENCIL_OP." };
    }
}

inline D3D12_BLEND GetUnderlyingBlend(BlendFunction op) {
    switch (op)
    {
    case BlendFunction::kZero:
        return D3D12_BLEND_ZERO;
    case BlendFunction::kOne:
        return D3D12_BLEND_ONE;
    case BlendFunction::kSrcColor:
        return D3D12_BLEND_SRC_COLOR;
    case BlendFunction::kOneMinusSrcColor:
        return D3D12_BLEND_INV_SRC_COLOR;
    case BlendFunction::kDstColor:
        return D3D12_BLEND_DEST_COLOR;
    case BlendFunction::kOneMinusDstColor:
        return D3D12_BLEND_INV_DEST_COLOR;
    case BlendFunction::kSrcAlpha:
        return D3D12_BLEND_SRC_ALPHA;
    case BlendFunction::kOneMinusSrcAlpha:
        return D3D12_BLEND_INV_SRC_ALPHA;
    case BlendFunction::kDstAlpha:
        return D3D12_BLEND_DEST_ALPHA;
    case BlendFunction::kOneMinusDstAlpha:
        return D3D12_BLEND_INV_DEST_ALPHA;
    case BlendFunction::kSrcAlphaSaturate:
    default:
        throw std::exception{ "Bad BlendFunction to D3D12_BLEND." };
    }
}

inline D3D12_BLEND_OP GetUnderlyingBlendOP(BlendEquation op) {
    switch (op)
    {
    case BlendEquation::kAdd:
        return D3D12_BLEND_OP_ADD;
    case BlendEquation::kSubtract:
        return D3D12_BLEND_OP_SUBTRACT;
    case BlendEquation::kReverseSubtract:
        return D3D12_BLEND_OP_REV_SUBTRACT;
    case BlendEquation::kMin:
        return D3D12_BLEND_OP_MIN;
    case BlendEquation::kMax:
        return D3D12_BLEND_OP_MAX;
    default:
        throw std::exception{ "Bad BlendEquation to D3D12_BLEND_OP." };
    }
}

inline D3D12_PRIMITIVE_TOPOLOGY_TYPE GetUnderlyingTopologyType(TopologyType topology) {
    switch (topology)
    {
    case TopologyType::kLine:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    case TopologyType::kPoint:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    case TopologyType::kTriangle:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    default:
        throw std::exception{ "Bad TopologyType to D3D12_BLEND." };
    }
}

inline D3D_PRIMITIVE_TOPOLOGY GetUnderlyingTopology(TopologyType topology) {
    switch (topology)
    {
    case TopologyType::kLine:
        return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
    case TopologyType::kPoint:
        return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
    case TopologyType::kTriangle:
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    default:
        throw std::exception{ "Bad TopologyType to D3D12_BLEND." };
    }
}

inline D3D12_FILTER GetUnderlyingFilter(FilterMode filter) {
    switch (filter)
    {
    case FilterMode::kBilinear:
        return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    case FilterMode::kTrilinear:
        return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    case FilterMode::kAnisotropic:
        return D3D12_FILTER_ANISOTROPIC;
    case FilterMode::kPoint:
        return D3D12_FILTER_MIN_MAG_MIP_POINT;
    case FilterMode::kCmpBilinear:
        return D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    case FilterMode::kCmpTrilinear:
        return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
    case FilterMode::kCmpAnisotropic:
        return D3D12_FILTER_COMPARISON_ANISOTROPIC;
    case FilterMode::kCmpPoint:
        return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
    default:
        throw std::exception{ "Bad Sampler Filter to D3D12_FILTER." };
    }
}

inline D3D12_TEXTURE_ADDRESS_MODE GetUnderlyingWrap(WarpMode wrap) {
    switch (wrap)
    {
    case WarpMode::kRepeat:
        return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    case WarpMode::kMirror:
        return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    case WarpMode::kClamp:
        return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    case WarpMode::kBorder:
        return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    default:
        throw std::exception{ "Bad Sampler Warp to D3D12_TEXTURE_ADDRESS_MODE." };
    }
}

// Get a UAV description that matches the resource description.
inline D3D12_UNORDERED_ACCESS_VIEW_DESC GetUavDesc(const D3D12_RESOURCE_DESC& resDesc, UINT mipSlice, UINT arraySlice = 0,
    UINT planeSlice = 0)
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = resDesc.Format;

    switch (resDesc.Dimension)
    {
    case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
        if (resDesc.DepthOrArraySize > 1)
        {
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
            uavDesc.Texture1DArray.ArraySize = resDesc.DepthOrArraySize - arraySlice;
            uavDesc.Texture1DArray.FirstArraySlice = arraySlice;
            uavDesc.Texture1DArray.MipSlice = mipSlice;
        }
        else
        {
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
            uavDesc.Texture1D.MipSlice = mipSlice;
        }
        break;
    case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
        if (resDesc.DepthOrArraySize > 1)
        {
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            uavDesc.Texture2DArray.ArraySize = resDesc.DepthOrArraySize - arraySlice;
            uavDesc.Texture2DArray.FirstArraySlice = arraySlice;
            uavDesc.Texture2DArray.PlaneSlice = planeSlice;
            uavDesc.Texture2DArray.MipSlice = mipSlice;
        }
        else
        {
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            uavDesc.Texture2D.PlaneSlice = planeSlice;
            uavDesc.Texture2D.MipSlice = mipSlice;
        }
        break;
    case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
        uavDesc.Texture3D.WSize = resDesc.DepthOrArraySize - arraySlice;
        uavDesc.Texture3D.FirstWSlice = arraySlice;
        uavDesc.Texture3D.MipSlice = mipSlice;
        break;
    default:
        throw std::exception("Invalid resource dimension.");
    }

    return uavDesc;
}

inline D3D12_RESOURCE_FLAGS GetCreateFlags(uint32_t flags) {
    uint32_t flag = 0;
    if (flags & (uint32_t)CreateFlags::kDepthStencil) {
        flag |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    }

    if (flags & (uint32_t)CreateFlags::kRenderTarget) {
        flag |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }
    
    if (flags & (uint32_t)CreateFlags::kUav) {
        flag |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    //if (flags | (uint32_t)CreateFlags::kShaderResource) {
    //    flag |= D3D12_RESOURCE_FLAG_NONE;
    //}

    return (D3D12_RESOURCE_FLAGS)flag;
}

inline D3D12_SHADER_VISIBILITY GetShaderVisibility(ShaderType type)
{
    D3D12_SHADER_VISIBILITY visibility;
    if (type == ShaderType::kVertex)
    {
        visibility = D3D12_SHADER_VISIBILITY_VERTEX;
    }
    else if (type == ShaderType::kPixel)
    {
        visibility = D3D12_SHADER_VISIBILITY_PIXEL;
    }
    else if (type == ShaderType::kCompute)
    {
        visibility = D3D12_SHADER_VISIBILITY_ALL;
    }
    else
    {
        assert(0);
    }

    return visibility;
}


}
}