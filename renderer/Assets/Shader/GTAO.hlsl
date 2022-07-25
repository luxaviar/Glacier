#include "PostProcessCommon.hlsl"
#include "Common/Colors.hlsli"
#include "Common/Utils.hlsli"

static const uint kNumAngles = 8;
static const uint kNumTaps = 10;
static const float kMaxScreenRadius = 256.0f;

cbuffer gtao_param {
    //[0]{temporal cos, temporal sin, temporal offset, temporal direction}
    //[1]{radius, fade_to_radius, thickness, fov_scale}
    float4 _GTAOParam[2];
};

Texture2D<float2> NormalTexture;
Texture2D DepthBuffer;

float3 ReconstructViewPos(float2 uv) {
    float depth = DepthBuffer.SampleLevel(linear_sampler, uv, 0).r;
    return ConstructPosition(uv, depth, _UnjitteredInverseProjection);
}

float3 GetRandomVector(int2 pos) {
    float noise = InterleavedGradientNoise(float2(pos));
    float2 cos_sin = float2(cos(noise * kPI), sin(noise * kPI));

    // Spatial Offsets and Directions - s2016_pbs_activision_occlusion - Slide 93
    float offset = 0.25 * ((pos.y - pos.x) & 3);

    float TemporalCos = _GTAOParam[0].x;
    float TemporalSin = _GTAOParam[0].y;

    return float3(
        dot(cos_sin.xy, float2(TemporalCos, -TemporalSin)),
        dot(cos_sin.xy, float2(TemporalSin, TemporalCos)),
        frac(offset + _GTAOParam[0].z)
    );
}

float2 SearchForLargestAngleDual(int steps, float2 center_uv, float2 screen_dir, float step_radius, float init_offset,
    float3 view_pos, float3 view_dir, float atten_factor)
{
    float2 h = float2(-1, -1);
    float thickness = _GTAOParam[1].z;

    for (int i = 0; i < steps; i++) {
        float2 uv_offset = screen_dir * max(step_radius * (i + init_offset), (i + 1.0));
        float4 uv = center_uv.xyxy + float4(uv_offset.xy, -uv_offset.xy);

        float3 ds = ReconstructViewPos(uv.xy) - view_pos;
        float3 dt = ReconstructViewPos(uv.zw) - view_pos;

        float2 dsdt = float2(dot(ds, ds), dot(dt, dt));
        float2 inv_length = rsqrt(dsdt + float2(0.0001, 0.0001));

        float2 H = float2(dot(ds, view_dir), dot(dt, view_dir)) *  inv_length;
        float2 attn = saturate(dsdt.xy * atten_factor);
        H = lerp(H, h, attn);
        
        h.x = (H.x > h.x) ? H.x : lerp(H.x, h.x, thickness);
        h.y = (H.y > h.y) ? H.y : lerp(H.y, h.y, thickness);
    }

    h = acos(clamp(h, -1.0f, 1.0f));

    return h;
}

float ComputeInnerIntegral(float2 h, float2 screen_dir, float3 view_dir, float3 view_normal, inout float3 bent_normal)
{
    float3 plane_normal = normalize(cross(float3(screen_dir, 0), view_dir));
    float3 plane_tangent = cross(view_dir, plane_normal);
    float3 proj_normal = view_normal - plane_normal * dot(view_normal, plane_normal);

    float proj_length = length(proj_normal) + 0.000001f;
    float cos_gamma = clamp(dot(proj_normal / proj_length, view_dir), -1.0f, 1.0f);
    float gamma = -sign(dot(proj_normal, plane_tangent)) * acos(cos_gamma);

    // clamp to normal hemisphere 
    h.x = gamma + max(-h.x - gamma, -kHalfPI);
    h.y = gamma + min(h.y - gamma, kHalfPI);

    float2 h2 = h * 2.0f;
    float sin_gamma = sin(gamma);

    float ao = proj_length * 0.25 *
            ((h2.x * sin_gamma + cos_gamma - cos(h2.x - gamma)) +
            (h2.y * sin_gamma + cos_gamma - cos(h2.y - gamma)));

    float3 bent_angle = (h.x + h.y) * 0.5f;
    bent_normal += (view_dir * cos(bent_angle) - plane_tangent * sin(bent_angle));

    return ao;
}

float ReflectionOcclusion(float3 bent_normal, float3 reflection_vector, float roughness, float ao)
{
    float bent_normal_length = length(bent_normal);

    float reflection_cone_angle = max(roughness, 0.04) * kPI;
    float unoccluded_angle = bent_normal_length * kPI * ao;
    float angle_between = acos(dot(bent_normal, reflection_vector) / max(bent_normal_length, 0.001));

    float ro = ConeConeIntersection(reflection_cone_angle, unoccluded_angle, angle_between);
    return lerp(0, ro, saturate((unoccluded_angle - 0.1) / 0.2));
}

float3 main_ps(float4 position : SV_Position, float2 uv : TEXCOORD) : SV_TARGET {
    float3 view_normal = DecodeNormalOct(NormalTexture.SampleLevel(linear_sampler, uv, 0).xy);
    view_normal.y = -view_normal.y;

    float3 view_position = ReconstructViewPos(uv);
    float3 view_dir = normalize(-view_position);

    int2 ipos = int2(position.xy);
    float3 random_vector = GetRandomVector(ipos);

    float world_radius = _GTAOParam[1].x;
    float world_radius_adj = world_radius * _GTAOParam[1].w;
    float screen_radius = max(min(world_radius_adj / view_position.z, kMaxScreenRadius), (float)kNumTaps);
    float step_radius = screen_radius / ((float)kNumTaps + 1);
    float atten_factor = 2.0f / (world_radius * world_radius);

    float kSinDeltaAngle = sin(kPI / (float)kNumAngles);
    float kCosDeltaAngle = cos(kPI / (float)kNumAngles);

    float2 screen_dir = random_vector.xy;
    float offset = random_vector.z;
    float3 bent_normal = float3(0, 0, 0);

    float sum = 0.0;
    for (uint i = 0; i < kNumAngles; i++) {
        float2 horizons = SearchForLargestAngleDual(kNumTaps, uv, screen_dir * _ScreenParam.zw, step_radius, offset,
            view_position, view_dir, atten_factor);

        sum += ComputeInnerIntegral(horizons, screen_dir, view_dir, view_normal, bent_normal);

        // Rotate for the next angle
        float2 temp_dir = screen_dir.xy;
        screen_dir.x = (temp_dir.x * kCosDeltaAngle) + (temp_dir.y * -kSinDeltaAngle);
        screen_dir.y = (temp_dir.x * kSinDeltaAngle) + (temp_dir.y * kCosDeltaAngle);
        offset = frac(offset + 0.617);
    }
    
    float ao = sum / (float)kNumAngles;
    //ao = lerp(1.0f, ao, ao_intensity);

    float3 reflect_dir = reflect(-view_dir, view_normal);
    bent_normal = normalize(normalize(bent_normal) - view_dir * 0.5f);

    float bent_normal_length = length(bent_normal);
    float unoccluded_angle = bent_normal_length * kPI * ao;
    float angle_between = acos(dot(bent_normal, reflect_dir) / max(bent_normal_length, 0.001));

    return float3(saturate(ao), unoccluded_angle, angle_between);
}