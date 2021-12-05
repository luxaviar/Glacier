struct VSOut
{
    float2 uv : Texcoord;
    float4 pos : SV_Position;
};

VSOut main_vs(float3 pos : Position)
{
    VSOut vso;
    vso.pos = float4(pos.x, pos.y, 0.0f, 1.0f);
    vso.uv = float2((pos.x + 1) / 2.0f, -(pos.y - 1) / 2.0f);
    return vso;
}

Texture2D tex : register(t0);
SamplerState sam : register(s0);
