#include "Common/Color.hlsli"
#include "Common/BasicSampler.hlsli"

StructuredBuffer<float> Exposure;
// Texture2D<float3> Bloom;
Texture2D<float4> SrcColor;
RWTexture2D<float4> DstColor;

cbuffer tonemap_params
{
    float2 RcpBufferDim;
    float BloomStrength;
    float PaperWhiteRatio; // PaperWhite / MaxBrightness
    float MaxBrightness;
};

[numthreads( 8, 8, 1 )]
void main_cs( uint3 DTid : SV_DispatchThreadID )
{
    float2 TexCoord = (DTid.xy + 0.5) * RcpBufferDim;

    // Load HDR and bloom
    float4 hdrColor = SrcColor[DTid.xy];

    // hdrColor += BloomStrength * Bloom.SampleLevel(_linear_clamp_sampler, TexCoord, 0);

    // Tone map to SDR
    float3 sdrColor = ACESToneMapping(hdrColor.rgb, Exposure[0]);
    sdrColor = float3(LinearToGammaSpace(sdrColor.r), LinearToGammaSpace(sdrColor.g), LinearToGammaSpace(sdrColor.b));
    DstColor[DTid.xy] = float4(sdrColor, hdrColor.a);
}
