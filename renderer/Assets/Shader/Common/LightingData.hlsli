#ifndef COMMON_LIGHTING_DATA_
#define COMMON_LIGHTING_DATA_

#include "Common/Transform.hlsli"
#include "Common/Shadow.hlsli"

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

SamplerComparisonState shadow_cmp_sampler;// : register(s0);
sampler linear_sampler;// : register(s1);
sampler point_sampler;// : register(s2);

#endif
