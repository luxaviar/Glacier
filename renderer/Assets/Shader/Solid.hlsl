#include "Common/Transform.hlsli"

float4 main_vs(float3 pos : Position) : SV_Position
{
    return mul(float4(pos, 1.0f), model_view_proj);
}

cbuffer color : register(b0)
{
    float4 materialColor;
};

float4 main_ps() : SV_Target
{
    return materialColor;
}