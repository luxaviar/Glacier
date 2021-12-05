#pragma once

#include <stdint.h>
#include "enums.h"

namespace glacier {
namespace render {

struct RasterState {
    RasterState() noexcept {
        static_assert(sizeof(RasterState) == sizeof(uint64_t),
            "RasterState size not what was intended");
        culling = CullingMode::kBack;
        topology = TopologyType::kTriangle;
        alphaToCoverage = false;
        scissor = false;

        depthWrite = true;
        depthFunc = CompareFunc::kLess;

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

    bool operator == (RasterState rhs) const noexcept { return u == rhs.u; }
    bool operator != (RasterState rhs) const noexcept { return u != rhs.u; }

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
        return !(depthWrite &&
            depthFunc == CompareFunc::kLess &&
            !stencilEnable &&
            stencilFunc == CompareFunc::kAlways &&
            stencilFailOp == StencilOp::kKeep &&
            depthFailOp == StencilOp::kKeep &&
            depthStencilPassOp == StencilOp::kKeep);
    }

    bool SameBlending(RasterState rs) const noexcept {
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

            bool stencilEnable : 1;                         //29
            CompareFunc stencilFunc : 3;      //32
            StencilOp stencilFailOp : 3;                   //35
            StencilOp depthFailOp : 3;                     //38
            StencilOp depthStencilPassOp : 3;              //41

            //! Whether color-buffer writes are enabled
            bool colorWrite : 1;        // 42

            //! use alpha-channel as coverage mask for anti-aliasing
            bool alphaToCoverage : 1;        // 43

            //! whether front face winding direction must be inverted
            bool inverseFrontFaces : 1;        // 44

            TopologyType topology : 2;          //46

            bool scissor : 1;                   //47

            //! padding, must be 0
            //uint8_t padding : 1;        // 48
        };
        uint64_t u = 0;
    };
};

}
}
