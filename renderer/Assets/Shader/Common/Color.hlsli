#ifndef COMMON_COLORS_
#define COMMON_COLORS_

// This assumes the default color gamut found in sRGB and REC709.  The color primaries determine these
// coefficients.  Note that this operates on linear values, not gamma space.
float RGBToLuminance(float3 col) {
    return dot(col, float3(0.212671, 0.715160, 0.072169));
}

//perceived
float LuminanceRec601(float3 col) {
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


float3 F(float3 x)
{
    const float A = 0.22f;
    const float B = 0.30f;
    const float C = 0.10f;
    const float D = 0.20f;
    const float E = 0.01f;
    const float F = 0.30f;
 
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

float3 Uncharted2ToneMapping(float3 color, float adapted_lum)
{
    const float WHITE = 11.2f;
    return F(1.6f * adapted_lum * color) / F(WHITE);
}

float3 ACESToneMapping(float3 color, float adapted_lum)
{
    const float A = 2.51f;
    const float B = 0.03f;
    const float C = 2.43f;
    const float D = 0.59f;
    const float E = 0.14f;

    color *= adapted_lum;
    return (color * (A * color + B)) / (color * (C * color + D) + E);
}

inline float LinearToGammaSpace(float value)
{
    if (value <= 0.0F)
        return 0.0F;
    else if (value <= 0.0031308F)
        return 12.92F * value;
    else if (value <= 1.0F)
        return 1.055F * pow(value, 0.41666F) - 0.055F;
    else
        return pow(value, 0.41666F);
}

float ComputeEV100FromAvgLuminance(float avgLuminance, float calibrationConstant)
{
    const float K = calibrationConstant;
    return log2(avgLuminance * 100.0 / K);
}

float ComputeEV100FromAvgLuminance(float avgLuminance)
{
    // We later use the middle gray at 12.7% in order to have
    // a middle gray at 18% with a sqrt(2) room for specular highlights
    // But here we deal with the spot meter measuring the middle gray
    // which is fixed at 12.5 for matching standard camera
    // constructor settings (i.e. calibration constant K = 12.5)
    // Reference: http://en.wikipedia.org/wiki/Film_speed
    const float K = 12.5; // Reflected-light meter calibration constant
    return ComputeEV100FromAvgLuminance(avgLuminance, K);
}

float ConvertEV100ToExposure(float EV100, float exposureScale)
{
    // Compute the maximum luminance possible with H_sbs sensitivity
    // maxLum = 78 / ( S * q ) * N^2 / t
    //        = 78 / ( S * q ) * 2^ EV_100
    //        = 78 / (100 * s_LensAttenuation) * 2^ EV_100
    //        = exposureScale * 2^ EV
    // Reference: http://en.wikipedia.org/wiki/Film_speed
    float maxLuminance = exposureScale * pow(2.0, EV100);
    return 1.0 / maxLuminance;
}

float ConvertEV100ToExposure(float EV100)
{
    const float exposureScale = 1.2;
    return ConvertEV100ToExposure(EV100, exposureScale);
}

#endif
