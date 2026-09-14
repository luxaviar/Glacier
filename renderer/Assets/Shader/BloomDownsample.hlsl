//13 tap downsample used to build the rest of the bloom pyramid, see
//"Next Generation Post Processing in Call of Duty: Advanced Warfare".
#include "PostProcessCommon.hlsl"
#include "BloomCommon.hlsli"

float3 GetBloomSource(float2 uv) {
    return _PostSourceTexture.SampleLevel(linear_sampler, uv, 0).rgb;
}

float4 main_ps(float4 position : SV_Position, float2 uv : Texcoord) : SV_TARGET {
    float2 t = _BloomTexelSize.xy;

    float3 a = GetBloomSource(uv + t * float2(-2.0, -2.0));
    float3 b = GetBloomSource(uv + t * float2( 0.0, -2.0));
    float3 c = GetBloomSource(uv + t * float2( 2.0, -2.0));
    float3 d = GetBloomSource(uv + t * float2(-2.0,  0.0));
    float3 e = GetBloomSource(uv);
    float3 f = GetBloomSource(uv + t * float2( 2.0,  0.0));
    float3 g = GetBloomSource(uv + t * float2(-2.0,  2.0));
    float3 h = GetBloomSource(uv + t * float2( 0.0,  2.0));
    float3 i = GetBloomSource(uv + t * float2( 2.0,  2.0));
    float3 j = GetBloomSource(uv + t * float2(-1.0, -1.0));
    float3 k = GetBloomSource(uv + t * float2( 1.0, -1.0));
    float3 l = GetBloomSource(uv + t * float2(-1.0,  1.0));
    float3 m = GetBloomSource(uv + t * float2( 1.0,  1.0));

    float3 color = e * 0.125;
    color += (a + c + g + i) * 0.03125;
    color += (b + d + f + h) * 0.0625;
    color += (j + k + l + m) * 0.125;

    return float4(color, 1.0);
}
