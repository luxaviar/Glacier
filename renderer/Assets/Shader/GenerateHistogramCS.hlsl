// The group size is 16x16, but one group iterates over an entire 16-wide column of pixels (384 pixels tall)
// Assuming the total workspace is 640x384, there will be 40 thread groups computing the histogram in parallel.
// The histogram measures logarithmic luminance ranging from 2^-12 up to 2^4.  This should provide a nice window
// where the exposure would range from 2^-4 up to 2^4.

#include "Common/BasicSampler.hlsli"
#include "Common/Color.hlsli"
#include "Common/Exposure.hlsli"

Texture2D<uint> LumaBuf : register( t0 );
RWByteAddressBuffer Histogram : register( u0 );

groupshared uint gs_TileHistogram[256];

[numthreads( 16, 16, 1 )]
void main_cs( uint GI : SV_GroupIndex, uint3 DTid : SV_DispatchThreadID )
{
    gs_TileHistogram[GI] = 0;

    GroupMemoryBarrierWithGroupSync();

    // Loop until the entire column has been processed
    for (uint2 ST = DTid.xy; ST.y < LumSize.y; ST.y += 16)
    {
        uint QuantizedLogLuma = LumaBuf[ST];
        InterlockedAdd( gs_TileHistogram[QuantizedLogLuma], 1 );
    }

    GroupMemoryBarrierWithGroupSync();

    Histogram.InterlockedAdd( GI * 4, gs_TileHistogram[GI] );
}
