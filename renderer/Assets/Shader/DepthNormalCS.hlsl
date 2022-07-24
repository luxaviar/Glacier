// This shader takes depth buffer and converts it to a flat normals buffer (world space)
//    Output is unsigned float3 in [0, 1] range

#include "Common/BasicBuffer.hlsli"
#include "Common/BasicTexture.hlsli"
#include "Common/BasicSampler.hlsli"
#include "Common/Utils.hlsli"

Texture2D<float> DepthBuffer;// : register(t0);

RWTexture2D<float2> NormalTexture;// : register(u0);

#define POSTPROCESS_BLOCKSIZE 8

static const uint TILE_BORDER = 1;
static const uint TILE_SIZE = POSTPROCESS_BLOCKSIZE + TILE_BORDER * 2;
static const uint2 TILE_SIZE_2D = uint2(TILE_SIZE, TILE_SIZE);

groupshared float2 tile_XY[TILE_SIZE * TILE_SIZE];
groupshared float tile_Z[TILE_SIZE * TILE_SIZE];

// 2D array index to flattened 1D array index
inline uint flatten2D(uint2 coord, uint2 dim)
{
    return coord.x + coord.y * dim.x;
}
// flattened array index to 2D array index
inline uint2 unflatten2D(uint idx, uint2 dim)
{
    return uint2(idx % dim.x, idx / dim.x);
}

[numthreads(POSTPROCESS_BLOCKSIZE, POSTPROCESS_BLOCKSIZE, 1)]
void main_cs(uint3 DTid : SV_DispatchThreadID, uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint groupIndex : SV_GroupIndex)
{
    const float depth_mip = 0;//postprocess.params0.x;

    const int2 tile_upperleft = Gid.xy * POSTPROCESS_BLOCKSIZE - TILE_BORDER;
    for (uint t = groupIndex; t < TILE_SIZE * TILE_SIZE; t += POSTPROCESS_BLOCKSIZE * POSTPROCESS_BLOCKSIZE)
    {
        const uint2 pixel = tile_upperleft + unflatten2D(t, TILE_SIZE_2D);
        const float2 uv = (pixel + 0.5f) * _ScreenParam.zw;
        const float depth = DepthBuffer.SampleLevel(linear_sampler, uv, depth_mip).r;
        const float3 position = ConstructPosition(uv, depth, _UnjitteredInverseProjection);
        tile_XY[t] = position.xy;
        tile_Z[t] = position.z;
    }
    GroupMemoryBarrierWithGroupSync();

    // reconstruct flat normals from depth buffer:
    //    Explanation: There are two main ways to reconstruct flat normals from depth buffer:
    //        1: use ddx() and ddy() on the reconstructed positions to compute triangle, this has artifacts on depth discontinuities and doesn't work in compute shader
    //        2: Take 3 taps from the depth buffer, reconstruct positions and compute triangle. This can still produce artifacts
    //            on discontinuities, but a little less. To fix the remaining artifacts, we can take 4 taps around the center, and find the "best triangle"
    //            by only computing the positions from those depths that have the least amount of discontinuity

    const uint cross_idx[5] = {
        flatten2D(TILE_BORDER + GTid.xy, TILE_SIZE_2D),                // 0: center
        flatten2D(TILE_BORDER + GTid.xy + int2(1, 0), TILE_SIZE_2D),    // 1: right
        flatten2D(TILE_BORDER + GTid.xy + int2(-1, 0), TILE_SIZE_2D),    // 2: left
        flatten2D(TILE_BORDER + GTid.xy + int2(0, 1), TILE_SIZE_2D),    // 3: down
        flatten2D(TILE_BORDER + GTid.xy + int2(0, -1), TILE_SIZE_2D),    // 4: up
    };

    const float center_Z = tile_Z[cross_idx[0]];

    [branch]
    if (center_Z >= _CameraParams.y)
         return;

    const uint best_Z_horizontal = abs(tile_Z[cross_idx[1]] - center_Z) < abs(tile_Z[cross_idx[2]] - center_Z) ? 1 : 2;
    const uint best_Z_vertical = abs(tile_Z[cross_idx[3]] - center_Z) < abs(tile_Z[cross_idx[4]] - center_Z) ? 3 : 4;

    float3 p1 = 0, p2 = 0;
    if (best_Z_horizontal == 1 && best_Z_vertical == 4)
    {
        p1 = float3(tile_XY[cross_idx[1]], tile_Z[cross_idx[1]]);
        p2 = float3(tile_XY[cross_idx[4]], tile_Z[cross_idx[4]]);
    }
    else if (best_Z_horizontal == 1 && best_Z_vertical == 3)
    {
        p1 = float3(tile_XY[cross_idx[3]], tile_Z[cross_idx[3]]);
        p2 = float3(tile_XY[cross_idx[1]], tile_Z[cross_idx[1]]);
    }
    else if (best_Z_horizontal == 2 && best_Z_vertical == 4)
    {
        p1 = float3(tile_XY[cross_idx[4]], tile_Z[cross_idx[4]]);
        p2 = float3(tile_XY[cross_idx[2]], tile_Z[cross_idx[2]]);
    }
    else if (best_Z_horizontal == 2 && best_Z_vertical == 3)
    {
        p1 = float3(tile_XY[cross_idx[2]], tile_Z[cross_idx[2]]);
        p2 = float3(tile_XY[cross_idx[3]], tile_Z[cross_idx[3]]);
    }

    const float3 P = float3(tile_XY[cross_idx[0]], tile_Z[cross_idx[0]]);
    const float3 normal = normalize(cross(p2 - P, p1 - P));

    NormalTexture[DTid.xy] = EncodeNormalOct(normal);
}
