#include "PostProcessCommon.hlsl"
#include "Common/Color.hlsli"
#include "Common/Utils.hlsli"
#include "Common/Ao.hlsli"

#define BLUR_RADIUS 6

cbuffer filter_param {
    float2 delta_direction;
    float2 padding;
};

// Input textures.
Texture2D OcclusionTexture;
Texture2D DepthBuffer;

inline float3 GetAo(float2 uv) {
    float3 ao = OcclusionTexture.SampleLevel(linear_sampler, uv, 0).rgb;
    return ao;
}

inline float GetDepth(float2 uv) {
    float depth = LinearDepth(DepthBuffer.SampleLevel(linear_sampler, uv, 0).r);
    return depth;
}

inline float CrossBilateralWeight(float r, float d, float d0) {
    const float BlurSigma = (float)BLUR_RADIUS * 0.5;
    const float BlurFalloff = 1 / (2 * BlurSigma * BlurSigma);

    float dz = (d0 - d) * _GTAOParam.sharpness;
    return exp2(-r * r * BlurFalloff - dz * dz);
}

float3 main_ps(float4 position : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    float2 delta_uv = delta_direction * _ScreenParam.zw;
    float center_depth = GetDepth(uv);
    float3 total_ao = GetAo(uv);
    float total_weight = 1;

    float r = 1.0;
    float2 offset_uv = 0.0;
    float3 ao;
    float depth, w;

    [unroll]
    for (; r <= BLUR_RADIUS / 2; r += 1) {
        offset_uv = uv + r * delta_uv;
        depth = GetDepth(offset_uv);
        ao = GetAo(offset_uv);
        w = CrossBilateralWeight(r, depth, center_depth);
        total_weight += w;
        total_ao += w * ao;

        offset_uv = uv - r * delta_uv;
        depth = GetDepth(offset_uv);
        ao = GetAo(offset_uv);
        w = CrossBilateralWeight(r, depth, center_depth);
        total_weight += w;
        total_ao += w * ao;
    }

    [unroll]
    for (; r <= BLUR_RADIUS; r += 2) {
        offset_uv = uv + (r + 0.5) * delta_uv;
        depth = GetDepth(offset_uv);
        ao = GetAo(offset_uv);
        w = CrossBilateralWeight(r, depth, center_depth);
        total_weight += w;
        total_ao += w * ao;

        offset_uv = uv - (r + 0.5) * delta_uv;
        depth = GetDepth(offset_uv);
        ao = GetAo(offset_uv);
        w = CrossBilateralWeight(r, depth, center_depth);
        total_weight += w;
        total_ao += w * ao;
    }

    ao = total_ao / total_weight;
    return ao;
}
