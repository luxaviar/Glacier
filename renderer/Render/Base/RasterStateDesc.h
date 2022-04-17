#pragma once

#include <stdint.h>
#include "enums.h"

namespace glacier {
namespace render {

struct RasterStateDesc {
#if GLACIER_REVERSE_Z
    static constexpr CompareFunc kDefaultDepthFunc = CompareFunc::kGreater;
    static constexpr CompareFunc kDefaultDepthFuncWithEqual = CompareFunc::kGreaterEqual;

    static constexpr CompareFunc kReverseDepthFunc = CompareFunc::kLess;
    static constexpr CompareFunc kReverseDepthFuncWithEqual = CompareFunc::kLessEqual;
#else
    static constexpr CompareFunc kDefaultDepthFunc = CompareFunc::kLess;
    static constexpr CompareFunc kDefaultDepthFuncWithEqual = CompareFunc::kLessEqual;

    static constexpr CompareFunc kReverseDepthFunc = CompareFunc::kGreater;
    static constexpr CompareFunc kReverseDepthFuncWithEqual = CompareFunc::kGreaterEqual;
#endif

    RasterStateDesc() noexcept {
        static_assert(sizeof(RasterStateDesc) == sizeof(uint64_t),
            "RasterState size not what was intended");
        culling = CullingMode::kBack;
        fillMode = FillMode::kSolid;
        topology = TopologyType::kTriangle;
        alphaToCoverage = false;
        scissor = false;

        depthWrite = true;
        depthFunc = kDefaultDepthFunc;

        depthEnable = true;
        stencilEnable = false;
        stencilFunc = CompareFunc::kAlways;
        stencilFailOp = StencilOp::kKeep;
        depthFailOp = StencilOp::kKeep;
        depthStencilPassOp = StencilOp::kKeep;

        blendEquationRGB = BlendEquation::kAdd;
        blendEquationAlpha = BlendEquation::kAdd;
        blendFunctionSrcRGB = BlendFunction::kOne;
        blendFunctionSrcAlpha = BlendFunction::kOne;
        blendFunctionDstRGB = BlendFunction::kZero;
        blendFunctionDstAlpha = BlendFunction::kZero;
    }

    bool operator == (RasterStateDesc rhs) const noexcept { return u == rhs.u; }
    bool operator != (RasterStateDesc rhs) const noexcept { return u != rhs.u; }

    void DisableBlending() noexcept {
        blendEquationRGB = BlendEquation::kAdd;
        blendEquationAlpha = BlendEquation::kAdd;
        blendFunctionSrcRGB = BlendFunction::kOne;
        blendFunctionSrcAlpha = BlendFunction::kOne;
        blendFunctionDstRGB = BlendFunction::kZero;
        blendFunctionDstAlpha = BlendFunction::kZero;
    }

    // note: clang reduces this entire function to a simple load/mask/compare
    bool HasBlending() const noexcept {
        // This is used to decide if blending needs to be enabled in the h/w
        return !(blendEquationRGB == BlendEquation::kAdd &&
            blendEquationAlpha == BlendEquation::kAdd &&
            blendFunctionSrcRGB == BlendFunction::kOne &&
            blendFunctionSrcAlpha == BlendFunction::kOne &&
            blendFunctionDstRGB == BlendFunction::kZero &&
            blendFunctionDstAlpha == BlendFunction::kZero);
    }

    bool HasStencilDepth() const noexcept {
        return !(depthWrite && depthEnable &&
            depthFunc == CompareFunc::kLess &&
            !stencilEnable &&
            stencilFunc == CompareFunc::kAlways &&
            stencilFailOp == StencilOp::kKeep &&
            depthFailOp == StencilOp::kKeep &&
            depthStencilPassOp == StencilOp::kKeep);
    }

    bool SameBlending(RasterStateDesc rs) const noexcept {
        return (blendEquationRGB == rs.blendEquationRGB &&
            blendEquationAlpha == rs.blendEquationAlpha &&
            blendFunctionSrcRGB == rs.blendFunctionSrcRGB &&
            blendFunctionSrcAlpha == rs.blendFunctionSrcAlpha &&
            blendFunctionDstRGB == rs.blendFunctionDstRGB &&
            blendFunctionDstAlpha == rs.blendFunctionDstAlpha);
    }

    uint32_t blend_state() {
        return (u << 2) >> 42;
    }

    union {
        struct {
            //! culling mode
            CullingMode culling : 2;        //  2

            //! blend equation for the red, green and blue components
            BlendEquation blendEquationRGB : 3;        //  5
            //! blend equation for the alpha component
            BlendEquation blendEquationAlpha : 3;        //  8
            //! blending function for the source color
            BlendFunction blendFunctionSrcRGB : 4;        // 12
            //! blending function for the source alpha
            BlendFunction blendFunctionSrcAlpha : 4;        // 16
            //! blending function for the destination color
            BlendFunction blendFunctionDstRGB : 4;        // 20
            //! blending function for the destination alpha
            BlendFunction blendFunctionDstAlpha : 4;        // 24

            //! Whether depth-buffer writes are enabled
            bool depthWrite : 1;        // 25
            //! Depth test function
            CompareFunc depthFunc : 3;        // 28

            bool depthEnable : 1;                           //29
            bool stencilEnable : 1;                         //30
            CompareFunc stencilFunc : 3;      //31
            StencilOp stencilFailOp : 3;                   //36
            StencilOp depthFailOp : 3;                     //39
            StencilOp depthStencilPassOp : 3;              //42

            //! Whether color-buffer writes are enabled
            bool colorWrite : 1;        // 43

            //! use alpha-channel as coverage mask for anti-aliasing
            bool alphaToCoverage : 1;        // 44

            //! whether front face winding direction must be inverted
            bool inverseFrontFaces : 1;        // 45

            TopologyType topology : 2;          //47

            bool scissor : 1;                   //48

            FillMode fillMode : 1;             //49

            //! padding, must be 0
            //uint8_t padding : 1;        // 50
        };
        uint64_t u = 0;
    };
};

}
}
