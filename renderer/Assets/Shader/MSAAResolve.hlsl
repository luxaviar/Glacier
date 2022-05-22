#include "PostProcessCommon.hlsl"
#include "Common/Colors.hlsli"

#ifndef MSAASamples_
    #define MSAASamples_ 1
#endif

Texture2DMS<float4> color_buffer : register(t0);
Texture2DMS<float> depth_buffer : register(t1);

struct PSOut
{
    float4 color : SV_Target;
    float depth : SV_Depth;
};

PSOut main_ps(float4 position : SV_Position, float2 uv : Texcoord)
{
    PSOut output;
    output.depth = depth_buffer.Load(int2(position.xy), 0);
    float4 color = 0;
    for(uint i = 0; i < MSAASamples_; i++)
    {
        float4 sub_sample = color_buffer.Load(int2(position.xy), i);
        color += FastTonemap(sub_sample);
    }

    color /= MSAASamples_;
    output.color = FastTonemapInvert(color);

    return output;
}
