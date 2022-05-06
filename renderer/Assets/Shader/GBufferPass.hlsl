#include "Common/Transform.hlsli"
#include "Common/Lighting.hlsli"

struct VertexOut
{
    float4 position  : SV_POSITION;
    float3 view_normal  : NORMAL;
    float3 view_tangent : TANGENT;
    float2 tex_coord : TEXCOORD;
};

struct FragOut
{
    float4 albedo_color    : SV_TARGET0;
    float2 view_normal  : SV_TARGET1;
    float4 ao_metal_roughness	: SV_TARGET2;
    float4 emissive     : SV_TARGET3;
};

struct PbrMaterial
{
    float3 f0;
    //--------------------------- ( 16 bytes )
    bool use_normal_map;
    //--------------------------- ( 16 bytes )
}; //--------------------------- ( 16 * 2 = 32 bytes )

cbuffer object_material : register(b3)
{
    PbrMaterial mat;
};

Texture2D albedo_tex : register(t4);
Texture2D normal_tex : register(t5);
Texture2D metalroughness_tex : register(t6);
Texture2D ao_tex : register(t7);
Texture2D emissive_tex : register(t8);

float2 OctWrap(float2 v)
{
    return (1.0 - abs(v.yx)) * (v.xy >= 0.0 ? 1.0 : -1.0);
}

float2 EncodeNormalOct(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    n.xy = n.z >= 0.0 ? n.xy : OctWrap(n.xy);
    n.xy = n.xy * 0.5 + 0.5;
    return n.xy;
}

VertexOut main_vs(AppData IN)
{
    VertexOut Out = (VertexOut)0.0f;
    Out.view_normal = mul(IN.normal, (float3x3)model_view);
    Out.view_tangent = mul(IN.tangent, (float3x3)model_view);
    Out.position = mul(float4(IN.position, 1.0f), model_view_proj);
    Out.tex_coord = IN.tex_coord * tex_ts.xy + tex_ts.zw;

    return Out;
}

FragOut main_ps(VertexOut IN)
{
    FragOut Out;

    // BaseColor
    Out.albedo_color = albedo_tex.Sample(linear_sampler, IN.tex_coord);

    ///TODO: use macro variant
    if (mat.use_normal_map)
    {
        float3 normal_vs = normalize(IN.view_normal);
        float3 tangent_vs = normalize(IN.view_tangent);
        float3 binormal_vs = normalize(cross(normal_vs, tangent_vs));
        float3x3 TBN = float3x3(tangent_vs, binormal_vs, normal_vs);
        float3 normal = DoNormalMapping(TBN, normal_tex, linear_sampler, IN.tex_coord);
        Out.view_normal = EncodeNormalOct(normal);
    }
    else {
        Out.view_normal = EncodeNormalOct(IN.view_normal);
    }

    float4 metal_roughness = metalroughness_tex.Sample(linear_sampler, IN.tex_coord);
    float metallic = metal_roughness.b;
    float roughness = metal_roughness.g;
    
    float ao = ao_tex.Sample(linear_sampler, IN.tex_coord).r;
    Out.ao_metal_roughness = float4(ao, roughness, metallic, 0.0f);

    // Emissive
    Out.emissive = float4(emissive_tex.Sample(linear_sampler, IN.tex_coord).rgb, 1.0f);

    return Out;
}
