cbuffer vp_matrix : register(b1)
{
    matrix vp;
};

struct VSOut
{
    float3 wpos : Position;
    float4 pos : SV_Position;
};

VSOut main_vs(float3 pos : Position)
{
    VSOut vso;
    vso.wpos = pos;
    vso.pos = mul(float4(pos, 0.0f), vp);

#ifdef GLACIER_REVERSE_Z
    vso.pos.z = 0.0f;
#else
    // make sure that the depth after w divide will be 1.0 (so that the z-buffering will work)
    vso.pos.z = vso.pos.w;
#endif
    return vso;
}

TextureCube tex : register(t0);
SamplerState tex_sam;// : register(s0);

float4 main_ps(float3 wpos : Position) : SV_TARGET
{
    float4 col = tex.Sample(tex_sam, wpos);
    return col;
}