#ifndef NUM_LIGHTS
#pragma message( "NUM_LIGHTS undefined. Default to 4.")
#define NUM_LIGHTS 4 // should be defined by the application.
#endif

#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1
#define SPOT_LIGHT 2

struct Light
{
    /**
    * Direction for directional lights (World space).
    */
    float3 direction; // or Position for point and spot lights
    /**
    * The type of the light.
    */
    uint type;
    //--------------------------------------------------------------( 16 bytes )
    /**
    * Direction for directional lights (View space).
    */
    float3 view_direction; // or Position for point and spot lights
    /**
    * The intensity of the light
    */
    float intensity;
    //--------------------------------------------------------------( 16 bytes )
    /**
    * Color of the light. Diffuse and specular colors are not seperated.
    */
    float4 color; //rgba
    //--------------------------------------------------------------( 16 bytes )
    
    matrix shadow_trans; //world -> shadow space
    //--------------------------------------------------------------( 16 x 4 bytes )
    float range;
    /**
    * Is the light enable
    */
    bool enable;
    bool shadow_enable;
    float padding;
    //--------------------------------------------------------------( 16 bytes )
    //--------------------------------------------------------------( 16 * 4 + 64 = 128 bytes )
};

struct AppData
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 tex_coord : TEXCOORD;
    float3 tangent : TANGENT;
    //float3 binormal : BINORMAL;
};

struct VSOut
{
    float3 world_position : POSITION0;
    float3 view_position : POSITION1;
    float3 view_normal : NORMAL;
    float3 view_tangent : TANGENT;
    //float3 view_binormal : BINORMAL;
    float4 position : SV_POSITION;
    float2 tex_coord : TEXCOORD0;
    //float3 shadow_coord : TEXCOORD1;
};

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
