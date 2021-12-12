#pragma once

namespace glacier {
namespace render {

enum class AttachmentPoint : uint8_t
{
    kColor0 = 0,         // Must be a uncompressed color format.
    kColor1,         // Must be a uncompressed color format.
    kColor2,         // Must be a uncompressed color format.
    kColor3,         // Must be a uncompressed color format.
    kColor4,         // Must be a uncompressed color format.
    kColor5,         // Must be a uncompressed color format.
    kColor6,         // Must be a uncompressed color format.
    kColor7,         // Must be a uncompressed color format.
    kDepthStencil,
    kNumAttachmentPoints
};

enum class MSAAType : uint8_t {
    kNone = 0,
    k2x = 2,
    k4x = 4,
    k8x = 8,
};

enum class IndexType : uint8_t {
    kUInt16,
    kUInt32,
};

//! Face culling Mode
enum class CullingMode : uint8_t {
    kNone,               //!< No culling, front and back faces are visible
    kFront,              //!< Front face culling, only back faces are visible
    kBack,               //!< Back face culling, only front faces are visible
    //FRONT_AND_BACK      //!< Front and Back, geometry is not visible
};

enum class FillMode : uint8_t {
    kSolid,
    kWireframe
};

enum class TopologyType : uint8_t {
    kTriangle = 0,
    kPoint,
    kLine,
    kLineStrip,
};

//! comparison function for the depth sampler
enum class CompareFunc : uint8_t {
    kLessEqual = 0,     //!< Less or equal
    kGreaterEqual,         //!< Greater or equal
    kLess,          //!< Strictly less than
    kGreater,          //!< Strictly greater than
    kEqual,          //!< Equal
    kNotEqual,         //!< Not equal
    kAlways,          //!< Always. Depth testing is deactivated.
    kNever           //!< Never. The depth test always fails.
};

enum class StencilOp : uint8_t {
    kKeep = 0,               //< Keep the stencil value
    kZero,               //< Set the stencil value to zero
    kReplace,            //< Replace the stencil value with the reference value
    kIncrease,           //< Increase the stencil value by one, wrap if necessary
    kIncreaseSaturate,   //< Increase the stencil value by one, clamp if necessary
    kDecrease,           //< Decrease the stencil value by one, wrap if necessary
    kDecreaseSaturate,   //< Decrease the stencil value by one, clamp if necessary
    kInvert              //< Invert the stencil data (bitwise not)
};

//! blending equation function
enum class BlendEquation : uint8_t {
    kAdd = 0,                    //!< the fragment is added to the color buffer
    kSubtract,               //!< the fragment is subtracted from the color buffer
    kReverseSubtract,       //!< the color buffer is subtracted from the fragment
    kMin,                    //!< the min between the fragment and color buffer
    kMax                     //!< the max between the fragment and color buffer
};

//! blending function
enum class BlendFunction : uint8_t {
    kZero = 0,                   //!< f(src, dst) = 0
    kOne,                    //!< f(src, dst) = 1
    kSrcColor,              //!< f(src, dst) = src
    kOneMinusSrcColor,    //!< f(src, dst) = 1-src
    kDstColor,              //!< f(src, dst) = dst
    kOneMinusDstColor,    //!< f(src, dst) = 1-dst
    kSrcAlpha,              //!< f(src, dst) = src.a
    kOneMinusSrcAlpha,    //!< f(src, dst) = 1-src.a
    kDstAlpha,              //!< f(src, dst) = dst.a
    kOneMinusDstAlpha,    //!< f(src, dst) = 1-dst.a
    kSrcAlphaSaturate      //!< f(src, dst) = (1,1,1) * min(src.a, 1 - dst.a), 1
};

enum class FilterMode : uint8_t {
    kBilinear,
    kTrilinear,
    kAnisotropic,
    kPoint,
    kCmpBilinear,
    kCmpTrilinear,
    kCmpPoint,
    kCmpAnisotropic,
};

enum class WarpMode : uint8_t {
    kRepeat,
    kMirror,
    kClamp,
    kBorder,
};

enum class UsageType : uint8_t {
    //kNone,
    kDefault,
    kImmutable,
    kDynamic,
    kStage,
};

enum class TextureFormat : uint8_t {
    kUnkown=0,
    kR8G8B8A8_UNORM,
    kR8G8B8A8_UNORM_SRGB,
    kR24G8_TYPELESS,
    kR24X8_TYPELESS,
    kD24S8_UINT,
    kR32_TYPELESS,
    kR32_FLOAT,
    kD32_FLOAT,
    kR24_UNORM_X8_TYPELESS,
    kR32G32B32A32_FLOAT,
    kR32G32B32A32_TYPELESS,
    kR16G16B16A16_FLOAT,
    kR16G16B16A16_TYPELESS,
};

enum class CubeFace : uint8_t {
    kRight = 0,  // +X
    kLeft,   // -X
    kTop,    // +Y
    kBottom, // -Y
    kFront,  // +Z
    kBack,   // -Z
    kCount,
};

enum class TextureType : uint8_t {
    kTexture1D,
    kTexture2D,
    kTexture3D,
    kTextureCube, //for D3D11
};

enum class ShaderType : uint8_t {
    kVertex = 0,
    kHull, //TessellationControl
    kDomain, //TessellationEvaluation
    kGeometry,
    kPixel,
    kCompute,
    kUnknown,
};

constexpr const char* DefaultShaderEntry[] = {"main_vs", "main_hs", "main_ds", "main_gs", "main_ps", "main_cs"};

enum class ShaderParameterCatetory : uint8_t {
    kVertex,
    kIndex,
    kCBV,
    kSRV,
    kUAV,
    kSampler,
    kUnknown = 0,
};

enum class MaterialPropertyType : uint8_t {
    kTexture,
    kSampler,
    kCBuffer,
    kColor,
    kFloat4,
    kMatrix,
};

enum class CameraType : uint8_t {
    kPersp = 0,
    kOrtho,
};

enum class QueryType : uint8_t {
    kTimer,
    kCountSamples,
    kCountSamplesPredicate,
    kCountStreamOutPrimitives,
    kCountTransformFeedbackPrimitives,
    kCountRenderPrimitives
};


}
}
