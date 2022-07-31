#include "PostProcessCommon.hlsl"
#include "Common/Ao.hlsli"

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

float3 main_ps(float4 position : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    float2 inv_size = _ScreenParam.zw;

    float3 topleft_ao = GetAo(uv);
    float3 topright_ao = GetAo(uv + (float2(0.0, 1.0) * inv_size ));
    float3 bottomleft_ao = GetAo(uv + (float2(1.0, 0.0) * inv_size));
    float3 bottomright_ao = GetAo(uv + (float2(1.0, 1.0) * inv_size));

    float topleft_depth = GetDepth(uv);
    float topright_depth = GetDepth(uv + (float2(0.0, 1.0) * inv_size));
    float bottomleft_depth = GetDepth(uv + (float2(1.0, 0.0) * inv_size));
    float bottomright_depth = GetDepth(uv + (float2(1.0, 1.0) * inv_size));

    float4 offset_depths = float4(topleft_depth, topright_depth, bottomleft_depth, bottomright_depth);
    float4 weights = saturate(1.0 - abs(offset_depths - topleft_depth) / _CameraParams.y);

    float2 fract_coord = frac(uv * _GTAOParam.render_param.xy);

    float3 filteredX0 = lerp(topleft_ao * weights.x, topright_ao * weights.y, fract_coord.x);
    float3 filteredX1 = lerp(bottomright_ao * weights.w, bottomleft_ao * weights.z, fract_coord.x);
    float3 filtered = lerp(filteredX0, filteredX1, fract_coord.y);

    return filtered;
}
