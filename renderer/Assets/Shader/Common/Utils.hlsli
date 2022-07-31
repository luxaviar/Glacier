#ifndef COMMON_UTILS_
#define COMMON_UTILS_

#include "Common/BasicBuffer.hlsli"

#define FLT_EPS 5.960464478e-8 //2^-24

static const float kPI = 3.141592653589793238462f;
static const float kHalfPI = 3.141592653589793238462f / 2.0f;

float2 OctWrap(float2 v)
{
    return (1.0 - abs(v.yx)) * (v.xy >= 0.0 ? 1.0 : -1.0);
}

float2 EncodeNormalOct(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    n.xy = n.z >= 0.0 ? n.xy : OctWrap(n.xy);
    n.xy = n.xy * 0.5 + 0.5;
    return n.xy;
}

float3 DecodeNormalOct(float2 f)
{
    f = f * 2.0 - 1.0;
    // https://twitter.com/Stubbesaurus/status/937994790553227264
    float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = saturate(-n.z);
    n.xy += n.xy >= 0.0 ? -t : t;
    return normalize(n);
}

float3 ConstructPosition(float2 uv, float depth, float4x4 inv_proj)
{
    //#ifdef GLACIER_REVERSE_Z
        //depth = 1 - depth;
    //#endif

    float4 ndc_position;
    ndc_position.xy = uv * 2.0f - 1.0f;
    ndc_position.y *= -1; //[0,0] is top left in directx
    ndc_position.z = depth;
    ndc_position.w = 1.0f;
    float4 view_position = mul(ndc_position, inv_proj);//_InverseProjection);
    return view_position.xyz / view_position.w;
}

// Jimenez's "Interleaved Gradient Noise"
float InterleavedGradientNoise(float2 pos) {
    return frac(52.9829189f * frac((pos.x * 0.06711056) + (pos.y * 0.00583715)));
}

// http://h14s.p5r.org/2012/09/0x5f3759df.html, [Drobot2014a] Low Level Optimizations for GCN, https://blog.selfshadow.com/publications/s2016-shading-course/activision/s2016_pbs_activision_occlusion.pdf slide 63
float FastSqrt(float x)
{
    return (float)(asfloat(0x1fbd1df5 + (asint(x) >> 1)));
}

// input [-1, 1] and output [0, PI], from https://seblagarde.wordpress.com/2014/12/01/inverse-trigonometric-functions-gpu-optimization-for-amd-gcn-architecture/
float FastACos(float inX)
{
    const float PI = 3.141593;
    const float HALF_PI = 1.570796;
    float x = abs(inX);
    float res = -0.156583 * x + HALF_PI;
    res *= FastSqrt(1.0 - x);
    return (inX >= 0) ? res : PI - res;
}

float LinearDepth(float z) {
    return 1.0 / (_ZBufferParams.z * z + _ZBufferParams.w);
}

float Linear01Depth(float z) {
    return 1.0 / (_ZBufferParams.x * z + _ZBufferParams.y);
}

float2 NDCToUV(float4 ndc_pos) {
    return float2(0.5 + 0.5 * ndc_pos.x, 0.5 - 0.5 * ndc_pos.y);
}

// Cubic filters naturually work in a [-2, 2] domain. For the resolve case we
// want to rescale the filter so that it works in [-1, 1] instead
float FilterCubic(in float x, in float B, in float C) {
    float y = 0.0f;
    float x2 = x * x;
    float x3 = x * x * x;
    if (x < 1)
        y = (12 - 9 * B - 6 * C) * x3 + (-18 + 12 * B + 6 * C) * x2 + (6 - 2 * B);
    else if (x <= 2)
        y = (-B - 6 * C) * x3 + (6 * B + 30 * C) * x2 + (-12 * B - 48 * C) * x + (8 * B + 24 * C);

    return y / 6.0f;
}

float Mitchell(in float x, in bool rescale) {
    x = rescale ? x * 2.0f : x;
    return FilterCubic(x, 1 / 3.0f, 1 / 3.0f);
}

float CatmullRom(in float x, in bool rescale) {
    x = rescale ? x * 2.0f : x;
    return FilterCubic(x, 0.0f, 0.5f);
}

#define USE_OPTIMIZATIONS 1

float3 ClipAABB(float3 aabb_min, float3 aabb_max, float3 col, float3 avg)
{
#if USE_OPTIMIZATIONS
    // note: only clips towards aabb center (but fast!)
    float3 p_clip = 0.5 * (aabb_max + aabb_min);
    float3 e_clip = 0.5 * (aabb_max - aabb_min);

    float3 v_clip = col - p_clip;
    float3 v_unit = v_clip.xyz / (e_clip + FLT_EPS);
    float3 a_unit = abs(v_unit);
    float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

    if (ma_unit > 1.0)
        return p_clip + v_clip / ma_unit;
    else
        return col;// point inside aabb
#else
    float4 r = col - avg;
    float3 rmax = aabb_max - avg.xyz;
    float3 rmin = aabb_min - avg.xyz;

    const float eps = FLT_EPS;

    if (r.x > rmax.x + eps)
        r *= (rmax.x / r.x);
    if (r.y > rmax.y + eps)
        r *= (rmax.y / r.y);
    if (r.z > rmax.z + eps)
        r *= (rmax.z / r.z);

    if (r.x < rmin.x - eps)
        r *= (rmin.x / r.x);
    if (r.y < rmin.y - eps)
        r *= (rmin.y / r.y);
    if (r.z < rmin.z - eps)
        r *= (rmin.z / r.z);

    return avg + r;
#endif
}

// https://gist.github.com/TheRealMJP/c83b8c0f46b63f3a88a5986f4fa982b1
// Samples a texture with Catmull-Rom filtering, using 9 texture fetches instead of 16.
// See http://vec3.ca/bicubic-filtering-in-fewer-taps/ for more details
float4 SampleTextureCatmullRom(in Texture2D<float4> tex, in SamplerState samp, in float2 uv, in float2 texDim)
{
    float2 samplePos = uv * texDim;
    float2 invTextureSize = 1.0 / texDim;
    float2 tc = floor(samplePos - 0.5f) + 0.5f;
    float2 f = samplePos - tc;
    float2 f2 = f * f;
    float2 f3 = f2 * f;

    float2 w0 = f2 - 0.5f * (f3 + f);
    float2 w1 = 1.5f * f3 - 2.5f * f2 + 1.f;
    float2 w3 = 0.5f * (f3 - f2);
    float2 w2 = 1 - w0 - w1 - w3;

    float2 w12 = w1 + w2;

    float2 tc0 = (tc - 1.f) * invTextureSize;
    float2 tc12 = (tc + w2 / w12) * invTextureSize;
    float2 tc3 = (tc + 2.f) * invTextureSize;

    float4 result =
        tex.SampleLevel(samp, float2(tc0.x, tc0.y), 0.f) * (w0.x * w0.y) +
        tex.SampleLevel(samp, float2(tc0.x, tc12.y), 0.f) * (w0.x * w12.y) +
        tex.SampleLevel(samp, float2(tc0.x, tc3.y), 0.f) * (w0.x * w3.y) +
        tex.SampleLevel(samp, float2(tc12.x, tc0.y), 0.f) * (w12.x * w0.y) +
        tex.SampleLevel(samp, float2(tc12.x, tc12.y), 0.f) * (w12.x * w12.y) +
        tex.SampleLevel(samp, float2(tc12.x, tc3.y), 0.f) * (w12.x * w3.y) +
        tex.SampleLevel(samp, float2(tc3.x, tc0.y), 0.f) * (w3.x * w0.y) +
        tex.SampleLevel(samp, float2(tc3.x, tc12.y), 0.f) * (w3.x * w12.y) +
        tex.SampleLevel(samp, float2(tc3.x, tc3.y), 0.f) * (w3.x * w3.y);

    return result;
}

#endif
