#include "Common/BasicBuffer.hlsli"

float4 main_vs(float3 pos : Position) : SV_Position
{
    return mul(float4(pos, 1.0f), _ModelViewProjection);
}

cbuffer color : register(b3)
{
    float4 materialColor;
};

float4 main_ps() : SV_Target
{
    return materialColor;
}