#ifndef COMMON_AO_
#define COMMON_AO_

#include "Common/Utils.hlsli"

struct GtaoParam {
    float temporal_cos;
    float temporal_sin;
    float temporal_offset;
    float temporal_direction;
    //--------------------------------------------------------------( 16 bytes )
    float radius;
    float fade_to_radius;
    float thickness;
    float fov_scale;
    //--------------------------------------------------------------( 16 bytes )
    float4 render_param;
    //--------------------------------------------------------------( 16 bytes )
    float intensity;
    float sharpness;
    int debug_ao;
    int debug_ro;
};

cbuffer _GtaoData {
    //[0]{temporal cos, temporal sin, temporal offset, temporal direction}
    //[1]{radius, fade_to_radius, thickness, fov_scale}
    GtaoParam _GTAOParam;
};

float3 MultiBounce(float ao, float3 albedo)
{
    float3 a = 2.0404 * albedo - 0.3324;
    float3 b = -4.7951 * albedo + 0.6417;
    float3 c = 2.7552 * albedo + 0.6903;
    return max(ao, ((ao * a + b) * ao + c) * ao);
}

float ConeConeIntersection(float arc_length0, float arc_length1, float angle_between_cones)
{
    float angle_difference = abs(arc_length0 - arc_length1);
    float angle_blend_alpha = saturate((angle_between_cones - angle_difference) / (arc_length0 + arc_length1 - angle_difference));
    return smoothstep(0, 1, 1 - angle_blend_alpha);
}

#endif
