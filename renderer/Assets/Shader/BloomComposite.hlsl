//Adds the final bloom texture on top of the hdr color frame, the tone mapping
//(which runs afterwards) picks the result up.
#include "PostProcessCommon.hlsl"
#include "BloomCommon.hlsli"

Texture2D<float4> BloomTexture;

float3 GetBloom(float2 uv) {
    return BloomTexture.SampleLevel(linear_sampler, uv, 0).rgb;
}

float4 main_ps(float4 position : SV_Position, float2 uv : Texcoord) : SV_TARGET {
    float2 t = _BloomParams.zw;

    float3 a = GetBloom(uv + t * float2(-1.0, -1.0));
    float3 b = GetBloom(uv + t * float2( 0.0, -1.0));
    float3 c = GetBloom(uv + t * float2( 1.0, -1.0));
    float3 d = GetBloom(uv + t * float2(-1.0,  0.0));
    float3 e = GetBloom(uv);
    float3 f = GetBloom(uv + t * float2( 1.0,  0.0));
    float3 g = GetBloom(uv + t * float2(-1.0,  1.0));
    float3 h = GetBloom(uv + t * float2( 0.0,  1.0));
    float3 i = GetBloom(uv + t * float2( 1.0,  1.0));

    float3 color = e * 0.25;
    color += (b + d + f + h) * 0.125;
    color += (a + c + g + i) * 0.0625;

    return float4(color * _BloomParams.y, 0.0);
}
