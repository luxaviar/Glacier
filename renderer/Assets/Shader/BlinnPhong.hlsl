#include "Common/Lighting.hlsli"
#include "Common/Shadow.hlsli"
#include "Common/Transform.hlsli"

struct PhongMaterial
{
    float4 ke;
    //--------------------------- ( 16 bytes )
    float4 ka;
    //--------------------------- ( 16 bytes )
    //float4 kd; //use albedo
    //float4 ks; //use albedo
    float4 ambient_color;
    //--------------------------- ( 16 bytes )
    float4 emissive_color;
    //--------------------------- ( 16 bytes )
    float gloss;
    bool use_normal_map;
    float2 padding;
    //--------------------------- ( 16 bytes )
}; //--------------------------- ( 16 * 4 = 64 bytes )

VSOut main_vs(AppData IN)
{
    VSOut vso;
    vso.world_position = (float3) mul(float4(IN.position, 1.0f), model);
    vso.view_position = (float3) mul(float4(IN.position, 1.0f), model_view);
    vso.view_normal = mul(IN.normal, (float3x3) model_view);
    vso.view_tangent = mul(IN.tangent, (float3x3) model_view);
    //vso.view_binormal = mul(IN.binormal, (float3x3) model_view);
    vso.position = mul(float4(IN.position, 1.0f), model_view_proj);
    vso.tex_coord = IN.tex_coord;
    
    //vso.shadowHomoPos = ToShadowHomoSpace(pos, model);
    return vso;
}

cbuffer object_material : register(b2)
{
    PhongMaterial mat;
};

Texture2D albedo_tex : register(t4);
Texture2D normal_tex : register(t5);

sampler linear_sampler : register(s1);

static const float4 CascadeColorsMultiplier[8] =
{
    float4(1.5f, 0.0f, 0.0f, 1.0f),
    float4(0.0f, 1.5f, 0.0f, 1.0f),
    float4(0.0f, 0.0f, 5.5f, 1.0f),
    float4(1.5f, 0.0f, 5.5f, 1.0f),
    float4(1.5f, 1.5f, 0.0f, 1.0f),
    float4(1.0f, 1.0f, 1.0f, 1.0f),
    float4(0.0f, 1.0f, 5.5f, 1.0f),
    float4(0.5f, 3.5f, 0.75f, 1.0f)
};

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
    
    float4 color = mat.ambient_color * mat.ka;
    color += mat.emissive_color * mat.ke;

    float3 eye = { 0, 0, 0};
    float3 P = IN.view_position;
    float3 V = normalize(eye - P);
    
    Light main_light = lights[0];
    float4 visualize_cascade_color = float4(1.0, 1.0, 1.0, 1.0);
    if (main_light.enable)
    {
        float shadow_level = 1.0f;
        if (main_light.shadow_enable)
        {
            shadow_level = CalcShadowLevel(main_light.view_direction, normal, IN.view_position, IN.world_position);
        }
        
        color += DoPhongLighting(main_light, mat.gloss, IN, V, P, normal) * shadow_level;
        //visualize_cascade_color = CascadeColorsMultiplier[cascade_index];
    }
    
    for (int i = 1; i < NUM_LIGHTS; ++i)
    {
        Light light = lights[i];
        if (light.enable)
        {
            color += DoPhongLighting(light, mat.gloss, IN, V, P, normal);
        }
    }
    // final color = attenuate diffuse & ambient by diffuse texture color and add specular reflected
    //return float4(saturate((diffuse + ambient) * dtex.rgb + specularReflected), 1.0f);
    return float4((color * albedo).rgb, albedo.a) * visualize_cascade_color;
}
