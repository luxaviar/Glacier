//Extracts the bright pixels of the hdr image while downsampling it by 2x. This
//is the first (and largest) mip of the bloom pyramid.
#include "PostProcessCommon.hlsl"
#include "BloomCommon.hlsli"

float3 GetBloomSource(float2 uv) {
    return _PostSourceTexture.SampleLevel(linear_sampler, uv, 0).rgb;
}

float4 main_ps(float4 position : SV_Position, float2 uv : Texcoord) : SV_TARGET {
    //every destination texel covers a 2x2 block of the source image, sample the
    //four source texels in the middle of that block
    float2 src_texel = _BloomTexelSize.xy;
    float2 uv00 = uv - 0.5 * src_texel;

    float3 color = 0.0;
    color += PrefilterBright(GetBloomSource(uv00));
    color += PrefilterBright(GetBloomSource(uv00 + float2(src_texel.x, 0.0)));
    color += PrefilterBright(GetBloomSource(uv00 + float2(0.0, src_texel.y)));
    color += PrefilterBright(GetBloomSource(uv00 + src_texel));

    return float4(color * 0.25, 1.0);
}
