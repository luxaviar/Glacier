// #include "common/Lighting.hlsli"

struct GzimoOut
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
};

cbuffer mvp_matrix : register(b0)
{
    matrix mvp;
};

float4 main_vs(float3 pos : POSITION) : SV_Position //, float3 color : COLOR, float2 coord : TEXCOORD) : SV_Position
{
    // GzimoOut go;
    // go.position = mul( float4(pos,1.0f),mvp );
    // go.color = color;
    // return go;
    return mul(float4(pos, 1.0f), mvp);
}

cbuffer line_color : register(b0)
{
   float4 materialColor;
};

float4 main_ps() : SV_Target
{
    return materialColor;//float4(go.color, 1.0f);
}
