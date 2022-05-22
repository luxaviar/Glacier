#ifndef COMMON_COLORS_
#define COMMON_COLORS_

#define USE_OPTIMIZATIONS 1
#define FLT_EPS 5.960464478e-8 //2^-24

// Rec709 for sRGB
float3 LuminanceRec709(float3 col) {
    return dot(col, float3(0.2126, 0.7152, 0.0722));
}

//perceived
float3 LuminanceRec601(float3 col) {
    return dot(col, float3(0.299, 0.587, 0.114));
}

// https://gpuopen.com/learn/optimized-reversible-tonemapper-for-resolve/
float3 FastTonemap(float3 col) {
    return col * rcp(max(col.r, max(col.g, col.b)) + 1.0);
}

float3 FastTonemapInvert(float3 col) {
    return col * rcp(1.0 - max(col.r, max(col.g, col.b)));
}

float4 FastTonemap(float4 col) {
    return float4(FastTonemap(col.rgb), col.a);
}

float4 FastTonemapInvert(float4 col) {
    return float4(FastTonemapInvert(col.rgb), col.a);
}

float3 RGB2YCoCgR(float3 col) {
    float3 result;

    result.y = col.r - col.b;
    float temp = col.b + result.y / 2;
    result.z = col.g - temp;
    result.x = temp + result.z / 2;

    return result;
}

float3 YCoCgR2RGB(float3 col) {
    float3 result;

    float temp = col.x - col.z / 2;
    result.g = col.z + temp;
    result.b = temp - col.y / 2;
    result.r = result.b + col.y;

    return result;
}

#endif
