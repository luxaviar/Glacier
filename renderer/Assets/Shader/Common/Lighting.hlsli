#ifndef COMMON_LIGHTING_
#define COMMON_LIGHTING_

#include "Common/LightingData.hlsli"
#include "Common/Brdf.hlsli"

// constants for opengl attenuation
#define ATTEN_CONST_FACTOR 1.000f
#define ATTEN_QUADRATIC_FACTOR 25.0f
// where the falloff down to zero should start
#define ATTEN_ZEROFADE_START 0.8f * 0.8f

float Attenuate(float distSqr, float range)
{
    // match the vertex lighting falloff
    float atten = 1 / (ATTEN_CONST_FACTOR + ATTEN_QUADRATIC_FACTOR / (range * range) * distSqr);

    // ...but vertex one does not falloff to zero at light's range; it falls off to 1/26 which
    // is then doubled by our shaders, resulting in 19/255 difference!
    // So force it to falloff to zero at the edges.
    if (distSqr >= ATTEN_ZEROFADE_START)
    {
        if (distSqr > 1)
            atten = 0;
        else
            atten *= 1 - (distSqr - ATTEN_ZEROFADE_START) / (1 - ATTEN_ZEROFADE_START);
    }

    return atten;
}

float3 DoNormalMapping(float3x3 TBN, Texture2D tex, sampler s, float2 uv)
{
    float3 normal = tex.Sample(s, uv).xyz;
    normal = normal * 2.0f - 1.0f;

    // Transform normal from tangent space to view space.
    normal = mul(normal, TBN);
    return normalize(normal);
}

float4 DoDiffuse(float3 L, float3 N, float4 light_color)
{
    float NdotL = max(dot(N, L), 0);
    return light_color * NdotL;
}

float4 DoSpecular(float3 L, float3 N, float3 V, float4 light_color, float gloss)
{
    float3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0);

    return light_color * pow(NdotH, gloss);
}

float4 DoPhongLighting(Light light, float gloss, VSOut IN, float3 V, float3 P, float3 N)
{
    float4 diffuse_color = 0;
    float4 specular_color = 0;

    float4 light_color = light.color * light.intensity;
    if (light.type == DIRECTIONAL_LIGHT)
    {
        float3 L = normalize(-light.view_direction);
        float NdotL = max(dot(N, L), 0);
        if (NdotL > 0)
        {
            diffuse_color = light_color * NdotL;
            specular_color = DoSpecular(L, N, V, light_color, gloss);
        }
    }
    else
    {
        float3 L = light.view_direction - P; //light position - position
        float dist = length(L);
        L /= dist;
        float NdotL = max(dot(N, L), 0);

        if (NdotL > 0)
        {
            //http://wiki.ogre3d.org/-Point+Light+Attenuation
            //https://geom.io/bakery/wiki/index.php?title=Point_Light_Attenuation
            //float attenuation = 1.0 / (1.0 + pow(dist / light.range * 5, 2));

            //https://imdoingitwrong.wordpress.com/2011/01/31/light-attenuation/     
            // calculate basic attenuation
            float denom = dist / light.range + 1;
            float attenuation = 1 / (denom * denom);
            float cutoff = 0.001;

            // scale and bias attenuation such that:
            //   attenuation == 0 at extent of max influence
            //   attenuation == 1 when d == 0
            attenuation = (attenuation - cutoff) / (1 - cutoff);
            attenuation = max(attenuation, 0);

            diffuse_color = light_color * NdotL * attenuation;
            specular_color = DoSpecular(L, N, V, light_color, gloss) * attenuation;
        }
    }

    return diffuse_color + specular_color;
}

// float DoDiffuse( float3 N, float3 L )
// {
//     return max( 0, dot( N, L ) );
// }

// float DoSpecular( float3 V, float3 N, float3 L )
// {
//     float3 R = normalize( reflect( -L, N ) );
//     float RdotV = max( 0, dot( R, V ) );

//     return pow( RdotV, MaterialCB.SpecularPower );
// }

// float DoAttenuation( float c, float l, float q, float d )
// {
//     return 1.0f / ( c + l * d + q * d * d );
// }

// float DoSpotCone( float3 spotDir, float3 L, float spotAngle )
// {
//     float minCos = cos( spotAngle );
//     float maxCos = ( minCos + 1.0f ) / 2.0f;
//     float cosAngle = dot( spotDir, -L );
//     return smoothstep( minCos, maxCos, cosAngle );
// }

// LightResult DoPointLight( PointLight light, float3 V, float3 P, float3 N )
// {
//     LightResult result;
//     float3 L = ( light.PositionVS.xyz - P );
//     float d = length( L );
//     L = L / d;

//     float attenuation = DoAttenuation( light.ConstantAttenuation,
//                                        light.LinearAttenuation,
//                                        light.QuadraticAttenuation,
//                                        d );

//     result.Diffuse = DoDiffuse( N, L ) * attenuation * light.Color;
//     result.Specular = DoSpecular( V, N, L ) * attenuation * light.Color;

//     return result;
// }

// LightResult DoSpotLight( SpotLight light, float3 V, float3 P, float3 N )
// {
//     LightResult result;
//     float3 L = ( light.PositionVS.xyz - P );
//     float d = length( L );
//     L = L / d;

//     float attenuation = DoAttenuation( light.ConstantAttenuation,
//                                        light.LinearAttenuation,
//                                        light.QuadraticAttenuation,
//                                        d );

//     float spotIntensity = DoSpotCone( light.DirectionVS.xyz, L, light.SpotAngle );

//     result.Diffuse = DoDiffuse( N, L ) * attenuation * spotIntensity * light.Color;
//     result.Specular = DoSpecular( V, N, L ) * attenuation * spotIntensity * light.Color;

//     return result;
// }


#endif
