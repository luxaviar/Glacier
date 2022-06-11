#include "PostProcessCommon.hlsl"
#include "Common/Colors.hlsli"
#include "Common/Utils.hlsli"

cbuffer taa_param {
    float4 _TAAConfig; //STATIC_BLENDING, DYNAMIC_BLENDING, MOTION_AMPLIFY
}

Texture2D<float4> PrevColorTexture;
Texture2D<float2> VelocityTexture;

static const uint kSampleCount = 9;
static const float kRcpSampleCount = 1.0f / kSampleCount;
static const float kVarianceClipGamma = 1.0f;

float4 main_ps(float4 position : SV_Position, float2 uv : Texcoord) : SV_TARGET {
    float3 total_curr_color = float3(0, 0, 0);
    float total_curr_weight = 0.0;
    float3 color_min = 10000;
    float3 color_max = -10000;
    float3 m1 = float3(0, 0, 0);
    float3 m2 = float3(0, 0, 0);
    float2 closest_uv = uv;

#ifdef GLACIER_REVERSE_Z
    float closest_depth = 0.0f;
#else
    float closest_depth = 1.0f;
#endif
 
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float2 neighbor_uv = uv + float2(x, y) * _ScreenParam.zw;
            float3 neighbor_color = _PostSourceTexture.Sample(point_sampler, neighbor_uv).rgb;
            neighbor_color = FastTonemap(neighbor_color);
            neighbor_color = RGB2YCoCgR(neighbor_color);
 
            float dist = length(float2(x, y));
            float weight = Mitchell(dist, false);
 
            total_curr_color += neighbor_color * weight;
            total_curr_weight += weight;
 
            color_min = min(color_min, neighbor_color);
            color_max = max(color_max, neighbor_color);
 
            m1 += neighbor_color;
            m2 += neighbor_color * neighbor_color;
            
            float neighbor_depth = _DepthBuffer.Sample(point_sampler, neighbor_uv).r;
#ifdef GLACIER_REVERSE_Z
            if (neighbor_depth > closest_depth)
#else
            if (neighbor_depth < closest_depth)
#endif
            {
                closest_depth = neighbor_depth;
                closest_uv = neighbor_uv;
            }
        }
    }

    float2 velocity = VelocityTexture.Sample(point_sampler, closest_uv).xy;
    float2 prev_uv = uv - velocity;

    float3 curr_color = total_curr_color / total_curr_weight;
    if (any(prev_uv != saturate(prev_uv))) {
        curr_color = FastTonemapInvert(YCoCgR2RGB(curr_color));
        return float4(curr_color, 1.0f);
    }

    float3 prev_color = SampleTextureCatmullRom(PrevColorTexture, point_sampler, prev_uv, _ScreenParam.xy).rgb;
    //float3 prev_color = SampleTextureCatmullRom(PrevColorTexture, linear_sampler, prev_uv, _ScreenParam.xy).rgb;

    prev_color = FastTonemap(prev_color);
    prev_color = RGB2YCoCgR(prev_color);

    // Variance clipping.
    float3 mu = m1 * kRcpSampleCount;
    float3 sigma = sqrt(abs(m2 * kRcpSampleCount - mu * mu));
    float3 minc = mu - kVarianceClipGamma * sigma;
    float3 maxc = mu + kVarianceClipGamma * sigma;

    prev_color = ClipAABB(minc, maxc, clamp(prev_color, color_min, color_max), mu);

    float3 ldr_curr_color = YCoCgR2RGB(curr_color);
    float3 ldr_prev_color = YCoCgR2RGB(prev_color);
 
    curr_color = FastTonemapInvert(ldr_curr_color);
    prev_color = FastTonemapInvert(ldr_prev_color);

    // high curr_weight for static object
    float prev_weight = clamp(
        lerp(_TAAConfig.x, _TAAConfig.y, length(velocity) * _TAAConfig.z),
        _TAAConfig.y, _TAAConfig.x
    );
    float curr_weight = 1.0 - prev_weight;

    //anti-flicker: Inverse Luminance Filtering
    curr_weight *= 1.0f / (1.0f + LuminanceRec709(ldr_curr_color));
    prev_weight *= 1.0f / (1.0f + LuminanceRec709(ldr_prev_color));
 
    float3 result = (curr_color * curr_weight + prev_color * prev_weight) / max(curr_weight + prev_weight, 0.00001);

    return float4(result, 1.0f);
}
