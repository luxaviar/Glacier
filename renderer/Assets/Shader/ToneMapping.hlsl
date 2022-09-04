#include "PostProcessCommon.hlsl"
#include "Common/Color.hlsli"

float4 main_ps(float4 position : SV_Position, float2 uv : Texcoord) : SV_Target
{
    const float gamma = 2.2;
    const float exposure = 1;
    float3 hdrColor = _PostSourceTexture.Sample(linear_sampler, uv).rgb;

    // Reinhard
    //float3 mapped = hdrColor / (hdrColor + 1.0f);
    
    //CryEngine
    //float3 mapped = 1.0f - exp(-hdrColor * exposure);
    //float3 mapped = Uncharted2ToneMapping(hdrColor, exposure);
    float3 mapped = ACESToneMapping(hdrColor, exposure);
    
    // Gamma correction
    //mapped = pow(mapped, 1.0f / gamma);
    mapped = float3(LinearToGammaSpace(mapped.r), LinearToGammaSpace(mapped.g), LinearToGammaSpace(mapped.b));

    return float4(mapped, 1.0);
}
