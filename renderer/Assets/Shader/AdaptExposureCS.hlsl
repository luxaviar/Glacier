#include "Common/Color.hlsli"
#include "Common/Exposure.hlsli"
#include "Common/BasicBuffer.hlsli"

// The group size is 16x16, but one group iterates over an entire 16-wide column of pixels (384 pixels tall)
// Assuming the total workspace is 640x384, there will be 40 thread groups computing the histogram in parallel.
// The histogram measures logarithmic luminance ranging from 2^-12 up to 2^4.  This should provide a nice window
// where the exposure would range from 2^-4 up to 2^4.

ByteAddressBuffer Histogram : register(t0);
RWStructuredBuffer<float> Exposure : register(u0);

groupshared float gs_Accum[256];

[numthreads( 256, 1, 1 )]
void main_cs( uint GI : SV_GroupIndex )
{
    float WeightedSum = (float)GI * (float)Histogram.Load(GI * 4);

    [unroll]
    for (uint i = 1; i < 256; i *= 2)
    {
        gs_Accum[GI] = WeightedSum;                 // Write
        GroupMemoryBarrierWithGroupSync();          // Sync
        WeightedSum += gs_Accum[(GI + i) % 256];    // Read
        GroupMemoryBarrierWithGroupSync();          // Sync
    }

    // If the entire image is black, don't adjust exposure
    if (WeightedSum == 0.0)
        return;

    if (GI == 0) {
        float LogRange = max(MaxExposure - MinExposure, 0.000001f);

        // Average histogram value is the weighted sum of all pixels divided by the total number of pixels
        // minus those pixels which provided no weight (i.e. black pixels.)
        float weightedHistAvg = WeightedSum / (max(1, PixelCount - Histogram.Load(0))) - 1.0; //[1, 255] -> [0, 254]
        float EV100 = weightedHistAvg / 254.0 * LogRange + MinExposure - ExposureCompensation;
        float prev_exposure = Exposure[1];

        float exposure = ComputeLuminanceAdaptation(prev_exposure, EV100, SpeedToLight, SpeedToDark, _DeltaTime);
        exposure = clamp(EV100, MinExposure, MaxExposure);

        Exposure[0] = ConvertEV100ToExposure(exposure);
        Exposure[1] = exposure;
    }
}
