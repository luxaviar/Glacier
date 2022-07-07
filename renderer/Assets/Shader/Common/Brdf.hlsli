#ifndef COMMON_BRDF_
#define COMMON_BRDF_

#include "Common/BasicBuffer.hlsli"

static const float PI = 3.14159265f;
static const float EPSILON = 1e-6f;

float Fd_Lambert()
{
    return 1.0f / PI;
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float D_GGX(float NoH, float roughness)
{
    float alpha = roughness * roughness;
    float a2 = alpha * alpha;
    float f = (NoH * NoH) * (a2 - 1.0f) + 1.0f;
    return a2 / (PI * f * f);
}

float3 F_Schlick(float VoH, float3 f0)
{
    float3 f90 = 1.0f;
    return f0 + (f90 - f0) * pow(max(1.0f - VoH, 0.0f), 5.0f);
}

float GeometrySchlickGGX(float NoV, float k)
{
    float nom = NoV;
    float denom = NoV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(float NoL, float NoV, float roughness)
{
    float k = (roughness + 1.0f) * (roughness + 1.0f) / 8.0f; //for direct lighting
    float ggx1 = GeometrySchlickGGX(NoV, k); // 视线方向的几何遮挡
    float ggx2 = GeometrySchlickGGX(NoL, k); // 光线方向的几何阴影

    return ggx1 * ggx2;
}

float3 DoPbrLighting(Light light, float3 P, float3 V, float3 N, float3 albedo, float3 f0, float roughness, float metallic)
{
    float4 diffuse_color = 0;
    float4 specular_color = 0;

    float3 light_color = light.color.rgb * light.intensity;
    float3 L = normalize(-light.view_direction);
    float3 color = 0.0f;

    if (light.type == POINT_LIGHT)
    {
        L = light.view_direction - P; //light position - position
        float dist = length(L);
        L /= dist;

        float denom = dist / light.range + 1;
        float attenuation = 1 / (denom * denom);
        float cutoff = 0.001;

        // scale and bias attenuation such that:
        //   attenuation == 0 at extent of max influence
        //   attenuation == 1 when d == 0
        attenuation = (attenuation - cutoff) / (1 - cutoff);
        attenuation = max(attenuation, 0);

        light_color *= attenuation;
    }

    float NoL = max(dot(N, L), 0);
    if (NoL > 0)
    {
        //diffuse_color = light_color * NdotL;
        //specular_color = DoSpecular(L, N, V, light_color, gloss);
        float3 H = normalize(L + V);
        float NoH = max(dot(N, H), 0);
        float NoV = max(dot(N, V), 0);
        float VoH = max(dot(V, H), 0);

        float3 diffuse = albedo * Fd_Lambert();
        float D = D_GGX(NoH, roughness);
        float3 F = F_Schlick(VoH, f0);
        float G = GeometrySmith(NoL, NoV, roughness);

        float denominator = 4.0 * NoV * NoL + 0.001f;
        float3 specular = (F * D * G) / denominator;

        // ks is equal to Fresnel
        float3 ks = F;
        // for energy conservation, the diffuse and specular light can't
        // be above 1.0 (unless the surface emits light); to preserve this
        // relationship the diffuse component (kd) should equal 1.0 - ks.
        float3 kd = 1.0f - ks;
        // multiply kd by the inverse metalness such that only non-metals 
        // have diffuse lighting, or a linear blend if partly metal (pure metals
        // have no diffuse light).
        kd *= (1.0 - metallic);

        // note that we already multiplied the BRDF by the Fresnel (ks) so we won't multiply by ks again
        color = (kd * diffuse + specular) * light_color * NoL;
    }

    return color;
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max((1.0f - roughness), F0) - F0) * pow(1.0f - cosTheta, 5.0f);
}

float3 EvaluateIBL(in TextureCube radiance_tex, in TextureCube irradiance_tex, in Texture2D brdf_lut_tex, in sampler linear_sampler,
    float3 V, float3 N, float3 f0, float3 albedo, float metallic, float roughness, float radianc_max_lod)
{
    //const float MAX_REFLECTION_LOD = 4.0;
    float3 inverseV = normalize(reflect(-V, N));
    float3 radiance = radiance_tex.SampleLevel(linear_sampler, inverseV, roughness * radianc_max_lod).rgb;

    float3 normal_ws = mul(N, (float3x3)_InverseView);
    float3 irradiance = irradiance_tex.Sample(linear_sampler, normal_ws).rgb;

    float VoN = max(dot(V, N), 0);
    float2 lut_uv = float2(VoN, roughness);
    lut_uv.y = 1.0f - lut_uv.y;
    float2 lut = brdf_lut_tex.Sample(linear_sampler, lut_uv).rg;

    float3 indirect_diffuse = irradiance * albedo;
    float3 indirect_specular = radiance * (f0 * lut.x + lut.y);

    float3 F = FresnelSchlickRoughness(VoN, f0, roughness);
    float3 ks = F;
    float3 kd = 1.0f - ks;
    kd *= (1.0f - metallic);

    float3 ambient = indirect_diffuse * kd + indirect_specular;
    return ambient;

}

#endif
