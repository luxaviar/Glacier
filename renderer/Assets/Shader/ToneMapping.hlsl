#include "PostProcessCommon.hlsl"

float3 F(float3 x)
{
    const float A = 0.22f;
    const float B = 0.30f;
    const float C = 0.10f;
    const float D = 0.20f;
    const float E = 0.01f;
    const float F = 0.30f;
 
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

float3 Uncharted2ToneMapping(float3 color, float adapted_lum)
{
    const float WHITE = 11.2f;
    return F(1.6f * adapted_lum * color) / F(WHITE);
}

float3 ACESToneMapping(float3 color, float adapted_lum)
{
    const float A = 2.51f;
    const float B = 0.03f;
    const float C = 2.43f;
    const float D = 0.59f;
    const float E = 0.14f;

    color *= adapted_lum;
    return (color * (A * color + B)) / (color * (C * color + D) + E);
}

inline float LinearToGammaSpace(float value)
{
    if (value <= 0.0F)
        return 0.0F;
    else if (value <= 0.0031308F)
        return 12.92F * value;
    else if (value <= 1.0F)
        return 1.055F * pow(value, 0.41666F) - 0.055F;
    else
        return pow(value, 0.41666F);
}

float4 main_ps(float4 position : SV_Position, float2 uv : Texcoord) : SV_Target
{
    const float gamma = 2.2;
    const float exposure = 1;
    float3 hdrColor = tex.Sample(tex_sam, uv).rgb;

    // Reinhard
    //float3 mapped = hdrColor / (hdrColor + 1.0f);
    
    //CryEngine
    //float3 mapped = 1.0f - exp(-hdrColor * exposure);
    //float3 mapped = Uncharted2ToneMapping(hdrColor, exposure);
    float3 mapped = ACESToneMapping(hdrColor, exposure);
    
    // Gamma校正
    //mapped = pow(mapped, 1.0f / gamma);
    mapped = float3(LinearToGammaSpace(mapped.r), LinearToGammaSpace(mapped.g), LinearToGammaSpace(mapped.b));

    return float4(mapped, 1.0);
}
