// The CS for extracting bright pixels and saving a log-luminance map (quantized to 8 bits).  This
// is then used to generate an 8-bit histogram.

#include "Common/BasicSampler.hlsli"
#include "Common/Color.hlsli"
#include "Common/Exposure.hlsli"

Texture2D<float3> SourceTexture;
StructuredBuffer<float> Exposure;
RWTexture2D<uint> LumaResult;

[numthreads( 8, 8, 1 )]
void main_cs( uint3 DTid : SV_DispatchThreadID )
{
    // We need the scale factor and the size of one pixel so that our four samples are right in the middle
    // of the quadrant they are covering.
    float2 uv = DTid.xy * LumSize.zw;
    float2 offset = LumSize.zw * 0.25f;

    // Use 4 bilinear samples to guarantee we don't undersample when downsizing by more than 2x
    float3 color1 = SourceTexture.SampleLevel(_linear_clamp_sampler, uv + float2(-offset.x, -offset.y), 0);
    float3 color2 = SourceTexture.SampleLevel(_linear_clamp_sampler, uv + float2( offset.x, -offset.y), 0);
    float3 color3 = SourceTexture.SampleLevel(_linear_clamp_sampler, uv + float2(-offset.x,  offset.y), 0);
    float3 color4 = SourceTexture.SampleLevel(_linear_clamp_sampler, uv + float2( offset.x,  offset.y), 0);

    // Compute average luminance
    float luma = RGBToLuminance(color1 + color2 + color3 + color4) * 0.25;

    // Prevent log(0) and put only pure black pixels in Histogram[0]
    if (luma == 0.0)
    {
        LumaResult[DTid.xy] = 0;
    }
    else
    {
        float EV100 = ComputeEV100FromAvgLuminance(luma, MeterCalibrationConstant);
        float logLuma = saturate((EV100 - MinExposure) * RcpExposureRange);    // Rescale to [0.0, 1.0]
        LumaResult[DTid.xy] = logLuma * 254.0 + 1.0;                    // Rescale to [1, 255]
    }
}
