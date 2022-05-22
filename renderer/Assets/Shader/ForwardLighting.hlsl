#include "Common/BasicSampler.hlsli"
#include "Common/BasicBuffer.hlsli"
#include "Common/BasicTexture.hlsli"
#include "Common/Lighting.hlsli"

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

VSOut main_vs(AppData IN)
{
    VSOut vso;
    vso.world_position = (float3) mul(float4(IN.position, 1.0f), _Model);
    vso.view_position = (float3) mul(float4(IN.position, 1.0f), _ModelView);
    vso.view_normal = mul(IN.normal, (float3x3) _ModelView);
    vso.view_tangent = mul(IN.tangent, (float3x3) _ModelView);
    //vso.view_binormal = mul(IN.binormal, (float3x3) _ModelView);
    vso.position = mul(float4(IN.position, 1.0f), _ModelViewProjection);
    vso.tex_coord = IN.tex_coord * _TextureTileScale.xy + _TextureTileScale.zw;
    
    //vso.shadowHomoPos = ToShadowHomoSpace(pos, _Model);
    return vso;
}

float4 main_ps(VSOut IN) : SV_Target
{
    float4 albedo = albedo_tex.Sample(linear_sampler, IN.tex_coord);
    float3 normal = IN.view_normal;
    ///TODO: use macro variant
    if (mat.use_normal_map)
    {
        float3 normal_vs = normalize(IN.view_normal);
        float3 tangent_vs = normalize(IN.view_tangent);
        float3 binormal_vs = normalize(cross(normal_vs, tangent_vs));
        float3x3 TBN = float3x3(tangent_vs, binormal_vs, normal_vs);
        normal = DoNormalMapping(TBN, normal_tex, linear_sampler, IN.tex_coord);
    }
    
    float4 metal_roughness = metalroughness_tex.Sample(linear_sampler, IN.tex_coord);
    float metallic = metal_roughness.b;
    float roughness = metal_roughness.g;
    
    float ao = ao_tex.Sample(linear_sampler, IN.tex_coord).r;
    float3 f0 = lerp(mat.f0, albedo.rgb, metallic);
    
    float3 final_color = emissive_tex.Sample(linear_sampler, IN.tex_coord).rgb;

    float3 eye = { 0, 0, 0 };
    float3 P = IN.view_position;
    float3 V = normalize(eye - P);
    
    Light main_light = lights[0];
    float4 visualize_cascade_color = float4(1.0, 1.0, 1.0, 1.0);
    if (main_light.enable)
    {
        float shadow_level = 1.0f;
        if (main_light.shadow_enable)
        {
            shadow_level = CalcShadowLevel(main_light.view_direction, normal, IN.view_position, IN.world_position,
                _ShadowParam, _ShadowTexture, shadow_cmp_sampler);
        }
        
        //final_color += DoLighting(main_light, mat.gloss, IN, V, P, normal) * shadow_level;
        final_color += DoPbrLighting(main_light, P, V, normal, albedo.rgb, f0, roughness, metallic) * shadow_level;
    }
    
    for (int i = 1; i < NUM_LIGHTS; ++i)
    {
        Light light = lights[i];
        if (light.enable)
        {
            final_color += DoPbrLighting(main_light, P, V, normal, albedo.rgb, f0, roughness, metallic);
        }
    }
    
    float3 ambient = EvaluateIBL(_IrradianceTextureCube, _IrradianceTextureCube, _BrdfLutTexture, linear_sampler,
        V, normal, f0, albedo.rgb, metallic, roughness, radiance_max_lod);
    ambient *= ao;

    final_color += ambient;
    
    return float4(final_color.rgb, albedo.a) * visualize_cascade_color;
}
