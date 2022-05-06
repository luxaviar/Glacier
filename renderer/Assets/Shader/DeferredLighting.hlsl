#include "Common/Lighting.hlsli"
#include "PostProcessCommon.hlsl"

cbuffer frame_data
{
    matrix inverse_projection;
    matrix inverse_view;
};

Texture2D albedo_tex;
Texture2D<float2> normal_tex;
Texture2D ao_metalroughness_tex;
Texture2D emissive_tex;

float3 DecodeNormalOct(float2 f)
{
    f = f * 2.0 - 1.0;
    // https://twitter.com/Stubbesaurus/status/937994790553227264
    float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = saturate(-n.z);
    n.xy += n.xy >= 0.0 ? -t : t;
    return normalize(n);
}

float3 ComputeViewPosition(float2 texcoord, float depth)
{
    #ifdef GLACIER_REVERSE_Z
        depth = 1 - depth;
    #endif

    float4 ndc_position;
    ndc_position.xy = texcoord * 2.0f - 1.0f;
    ndc_position.y *= -1; //[0,0] is top left in directx
    ndc_position.z = depth;
    ndc_position.w = 1.0f;
    float4 view_position = mul(ndc_position, inverse_projection);
    return view_position.xyz / view_position.w;
}

float4 main_ps(float4 position : SV_Position, float2 uv : Texcoord) : SV_TARGET
{
    float4 albedo = albedo_tex.Sample(linear_sampler, uv);
    float3 normal = DecodeNormalOct(normal_tex.Sample(linear_sampler, uv).xy);
    float3 emissive = emissive_tex.Sample(linear_sampler, uv).rgb;
    float3 ao_metalroughness = ao_metalroughness_tex.Sample(linear_sampler, uv).rgb;

    float depth = DepthBuffer_.Sample(linear_sampler, uv).r;
    float3 view_position = ComputeViewPosition(uv, depth);
    float3 world_position = (float3)mul(float4(view_position, 1.0f), inverse_view);

    //normal = normalize(normal);
    float ao = ao_metalroughness.r;
    float roughness = ao_metalroughness.g;
    float metallic = ao_metalroughness.b;
    float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo.rgb, metallic);
    
    float3 final_color = emissive;
    //float3 view_position = (float3)mul(float4(world_position, 1.0f), view_matrix);

    float3 eye = { 0, 0, 0 };
    float3 P = view_position;
    float3 V = normalize(eye - P);
    
    Light main_light = lights[0];
    float4 visualize_cascade_color = float4(1.0, 1.0, 1.0, 1.0);
    if (main_light.enable)
    {
        float shadow_level = 1.0f;
        if (main_light.shadow_enable)
        {
            shadow_level = CalcShadowLevel(main_light.view_direction, normal, view_position, world_position,
                shadow_info, ShadowTexture_, shadow_cmp_sampler);
        }

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
    
    float3 ambient = EvaluateIBL(RadianceTextureCube_, IrradianceTextureCube_, BrdfLutTexture_, linear_sampler,
        V, normal, f0, albedo.rgb, metallic, roughness, radiance_max_lod);
    ambient *= ao;

    final_color += ambient;
    
    return float4(final_color.rgb, albedo.a) * visualize_cascade_color;
}