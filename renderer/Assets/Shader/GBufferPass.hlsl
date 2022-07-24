#include "Common/BasicBuffer.hlsli"
#include "Common/BasicSampler.hlsli"
#include "Common/Lighting.hlsli"
#include "Common/Utils.hlsli"

struct VertexOut
{
    float4 position  : SV_POSITION;
    float3 view_normal  : NORMAL;
    float3 view_tangent : TANGENT;
    float2 tex_coord : TEXCOORD;
    float4 cur_position : POSITION0;
    float4 prev_position : POSITION1;
};

struct FragOut
{
    float4 albedo_color    : SV_TARGET0;
    float2 view_normal  : SV_TARGET1;
    float4 ao_metal_roughness	: SV_TARGET2;
    float4 emissive     : SV_TARGET3;
    float2 velocity     : SV_TARGET4;
};

struct PbrMaterial
{
    float3 f0;
    //--------------------------- ( 16 bytes )
    bool use_normal_map;
    //--------------------------- ( 16 bytes )
}; //--------------------------- ( 16 * 2 = 32 bytes )

cbuffer object_material// : register(b3)
{
    PbrMaterial mat;
};

Texture2D AlbedoTexture;// : register(t4);
Texture2D NormalTexture;// : register(t5);
Texture2D MetalRoughnessTexture;// : register(t6);
Texture2D AoTexture;// : register(t7);
Texture2D EmissiveTexture;// : register(t8);

VertexOut main_vs(AppData IN)
{
    VertexOut Out = (VertexOut)0.0f;
    float4 wpos = mul(float4(IN.position, 1.0f), _Model);
    Out.view_normal = mul(IN.normal, (float3x3)_ModelView);
    Out.view_tangent = mul(IN.tangent, (float3x3)_ModelView);
    Out.position = mul(wpos, _ViewProjection);
    Out.cur_position = mul(wpos, _UnjitteredViewProjection);
    Out.prev_position = mul(mul(float4(IN.position, 1.0f), _PrevModel), _PrevViewProjection);

    Out.tex_coord = IN.tex_coord * _TextureTileScale.xy + _TextureTileScale.zw;

    return Out;
}

FragOut main_ps(VertexOut IN)
{
    FragOut Out = (FragOut)0;

    // BaseColor
    float4 albedo_color = AlbedoTexture.Sample(linear_sampler, IN.tex_coord);
    if (albedo_color.a == 0.0f) {
        discard;
    }

    Out.albedo_color = albedo_color;

    ///TODO: use macro variant
    if (mat.use_normal_map)
    {
        float3 normal_vs = normalize(IN.view_normal);
        float3 tangent_vs = normalize(IN.view_tangent);
        float3 binormal_vs = normalize(cross(normal_vs, tangent_vs));
        float3x3 TBN = float3x3(tangent_vs, binormal_vs, normal_vs);
        float3 normal = DoNormalMapping(TBN, NormalTexture, linear_sampler, IN.tex_coord);
        Out.view_normal = EncodeNormalOct(normal);
    }
    else {
        Out.view_normal = EncodeNormalOct(IN.view_normal);
    }

    float4 metal_roughness = MetalRoughnessTexture.Sample(linear_sampler, IN.tex_coord);
    float metallic = metal_roughness.b;
    float roughness = metal_roughness.g;
    
    float ao = AoTexture.Sample(linear_sampler, IN.tex_coord).r;
    Out.ao_metal_roughness = float4(ao, roughness, metallic, 0.0f);

    // Emissive
    Out.emissive = float4(EmissiveTexture.Sample(linear_sampler, IN.tex_coord).rgb, 1.0f);

    // Velocity
    float4 cur_pos = IN.cur_position;
    cur_pos /= cur_pos.w;
    cur_pos.xy = NDCToUV(cur_pos);
    
    float4 prev_pos = IN.prev_position;
    prev_pos /= prev_pos.w;
    prev_pos.xy = NDCToUV(prev_pos);

    Out.velocity = cur_pos.xy - prev_pos.xy;

    return Out;
}
