#include "PostProcessCommon.hlsl"

cbuffer Kernel : register(b3)
{
    uint nTaps;
    //float coefficients[15];
    float4 coefficients_packed[4];
}

cbuffer Control : register(b4)
{
    bool horizontal;
}

static float coefficients[16] = (float[16])coefficients_packed;

float4 main_ps(float4 position : SV_Position, float2 uv : Texcoord) : SV_Target
{
    float width, height;
    PostSourceTexture_.GetDimensions(width, height);
    float dx, dy;
    if (horizontal)
    {
        dx = 1.0f / width;
        dy = 0.0f;
    }
    else
    {
        dx = 0.0f;
        dy = 1.0f / height;
    }
    const int r = nTaps / 2;
    
    float accAlpha = 0.0f;
    float3 maxColor = float3(0.0f, 0.0f, 0.0f);
    
    [unroll(128)]
    for (int i = -r; i <= r; i++)
    {
        const float2 tc = uv + float2(dx * i, dy * i);
        const float4 s = PostSourceTexture_.Sample(point_sampler, tc).rgba;
        const float coef = coefficients[i + r];
        accAlpha += s.a * coef;
        maxColor = max(s.rgb, maxColor);
    }
    return float4(maxColor, accAlpha);
}