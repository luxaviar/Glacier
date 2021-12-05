#pragma once

#include <d3d11_1.h>
#include "Render/Base/Enums.h"
#include "Render/Base/Util.h"

namespace glacier {
namespace render {

inline D3D11_USAGE ToUsage(UsageType type) {
    switch (type)
    {
    case UsageType::kDefault:
        return D3D11_USAGE_DEFAULT;
    case UsageType::kImmutable:
        return D3D11_USAGE_IMMUTABLE;
    case UsageType::kDynamic:
        return D3D11_USAGE_DYNAMIC;
    case UsageType::kStage:
        return D3D11_USAGE_STAGING;
    default:
        throw std::exception{ "Bad UsageType to D3D11_USAGE." };
    }
}

inline D3D11_FILTER GetUnderlyingFilter(FilterMode filter) {
    switch (filter)
    {
    case FilterMode::kBilinear:
        return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    case FilterMode::kTrilinear:
        return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    case FilterMode::kAnisotropic:
        return D3D11_FILTER_ANISOTROPIC;
    case FilterMode::kPoint:
        return D3D11_FILTER_MIN_MAG_MIP_POINT;
    case FilterMode::kCmpBilinear:
        return D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    case FilterMode::kCmpTrilinear:
        return D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
    case FilterMode::kCmpAnisotropic:
        return D3D11_FILTER_COMPARISON_ANISOTROPIC;
    case FilterMode::kCmpPoint:
        return D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
    default:
        throw std::exception{ "Bad Sampler Filter to D3D11_FILTER." };
    }
}

inline D3D11_TEXTURE_ADDRESS_MODE GetUnderlyingWrap(WarpMode wrap) {
    switch (wrap)
    {
    case WarpMode::kRepeat:
        return D3D11_TEXTURE_ADDRESS_WRAP;
    case WarpMode::kMirror:
        return D3D11_TEXTURE_ADDRESS_MIRROR;
    case WarpMode::kClamp:
        return D3D11_TEXTURE_ADDRESS_CLAMP;
    case WarpMode::kBorder:
        return D3D11_TEXTURE_ADDRESS_BORDER;
    default:
        throw std::exception{ "Bad Sampler Warp to D3D11_TEXTURE_ADDRESS_MODE." };
    }
}

inline D3D11_COMPARISON_FUNC GetUnderlyingCompareFunc(CompareFunc cmp) {
    switch (cmp)
    {
    case CompareFunc::kNever:
        return D3D11_COMPARISON_NEVER;
    case CompareFunc::kLess:
        return D3D11_COMPARISON_LESS;
    case CompareFunc::kEqual:
        return D3D11_COMPARISON_EQUAL;
    case CompareFunc::kLessEqual:
        return D3D11_COMPARISON_LESS_EQUAL;
    case CompareFunc::kGreater:
        return D3D11_COMPARISON_GREATER;
    case CompareFunc::kNotEqual:
        return D3D11_COMPARISON_NOT_EQUAL;
    case CompareFunc::kGreaterEqual:
        return D3D11_COMPARISON_GREATER_EQUAL;
    case CompareFunc::kAlways:
        return D3D11_COMPARISON_ALWAYS;
    default:
        throw std::exception{ "Bad Sampler CmpFunc to D3D11_COMPARISON_FUNC." };
    }
}

inline D3D11_CULL_MODE GetUnderlyingCullMode(CullingMode mode) {
    switch (mode)
    {
    case CullingMode::kNone:
        return D3D11_CULL_NONE;
    case CullingMode::kFront:
        return D3D11_CULL_FRONT;
    case CullingMode::kBack:
        return D3D11_CULL_BACK;
    default:
        throw std::exception{ "Bad CullingMode to D3D11_CULL_MODE." };
    }
}

inline D3D11_BLEND_OP GetUnderlyingBlendOP(BlendEquation op) {
    switch (op)
    {
    case BlendEquation::kAdd:
        return D3D11_BLEND_OP_ADD;
    case BlendEquation::kSubtract:
        return D3D11_BLEND_OP_SUBTRACT;
    case BlendEquation::kReverseSubtract:
        return D3D11_BLEND_OP_REV_SUBTRACT;
    case BlendEquation::kMin:
        return D3D11_BLEND_OP_MIN;
    case BlendEquation::kMax:
        return D3D11_BLEND_OP_MAX;
    default:
        throw std::exception{ "Bad BlendEquation to D3D11_BLEND_OP." };
    }
}

inline D3D11_BLEND GetUnderlyingBlend(BlendFunction op) {
    switch (op)
    {
    case BlendFunction::kZero:
        return D3D11_BLEND_ZERO;
    case BlendFunction::kOne:
        return D3D11_BLEND_ONE;
    case BlendFunction::kSrcColor:
        return D3D11_BLEND_SRC_COLOR;
    case BlendFunction::kOneMinusSrcColor:
        return D3D11_BLEND_INV_SRC_COLOR;
    case BlendFunction::kDstColor:
        return D3D11_BLEND_DEST_COLOR;
    case BlendFunction::kOneMinusDstColor:
        return D3D11_BLEND_INV_DEST_COLOR;
    case BlendFunction::kSrcAlpha:
        return D3D11_BLEND_SRC_ALPHA;
    case BlendFunction::kOneMinusSrcAlpha:
        return D3D11_BLEND_INV_SRC_ALPHA;
    case BlendFunction::kDstAlpha:
        return D3D11_BLEND_DEST_ALPHA;
    case BlendFunction::kOneMinusDstAlpha:
        return D3D11_BLEND_INV_DEST_ALPHA;
    case BlendFunction::kSrcAlphaSaturate:
    default:
        throw std::exception{ "Bad BlendFunction to D3D11_BLEND." };
    }
}

inline D3D11_STENCIL_OP GetUnderlyingStencilOP(StencilOp op) {
    switch (op)
    {
    case StencilOp::kKeep:
        return D3D11_STENCIL_OP_KEEP;
    case StencilOp::kZero:
        return D3D11_STENCIL_OP_ZERO;
    case StencilOp::kReplace:
        return D3D11_STENCIL_OP_REPLACE;
    case StencilOp::kIncrease:
        return D3D11_STENCIL_OP_INCR;
    case StencilOp::kIncreaseSaturate:
        return D3D11_STENCIL_OP_INCR_SAT;
    case StencilOp::kDecrease:
        return D3D11_STENCIL_OP_DECR;
    case StencilOp::kDecreaseSaturate:
        return D3D11_STENCIL_OP_DECR_SAT;
    case StencilOp::kInvert:
        return D3D11_STENCIL_OP_INVERT;
    default:
        throw std::exception{ "Bad StencilOp to D3D11_STENCIL_OP." };
    }
}

inline D3D11_PRIMITIVE_TOPOLOGY GetTopology(TopologyType type) {
    switch (type)
    {
    case TopologyType::kTriangle:
        return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    case TopologyType::kPoint:
        return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
    case TopologyType::kLine:
        return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
    case TopologyType::kLineStrip:
        return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
    default:
        throw std::exception{ "Bad TopologyType to D3D11_PRIMITIVE_TOPOLOGY." };
    }
}


}
}