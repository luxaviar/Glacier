#include "Common/Transform.hlsli"
#include "Common/Lighting.hlsli"
#include "Common/Brdf.hlsli"
#include "Common/Shadow.hlsli"

cbuffer LightList : register(b1)
{
    Light lights[NUM_LIGHTS];
    float radiance_max_lod;
    float3 padding;
}

cbuffer CascadeShadowData : register(b2)
{
    CascadeShadowMapInfo shadow_info;
}

Texture2D shadow_tex : register(t0);
Texture2D brdf_lut_tex : register(t1);
TextureCube radiance_tex : register(t2);
TextureCube irradiance_tex : register(t3);

SamplerComparisonState shadow_cmp_sampler : register(s0);
sampler linear_sampler : register(s1);
