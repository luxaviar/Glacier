#ifndef COMMON_EXPOSURE_
#define COMMON_EXPOSURE_

cbuffer exposure_params
{
    float MinExposure;
    float MaxExposure;
    float RcpExposureRange;
    float ExposureCompensation;
    float4 LumSize;
    float MeterCalibrationConstant;
    float SpeedToLight;
    float SpeedToDark;
    uint PixelCount;
}

float ComputeLuminanceAdaptation(float previousLuminance, float currentLuminance, float speedDarkToLight, float speedLightToDark, float deltaTime)
{
    float delta = currentLuminance - previousLuminance;
    float speed = delta > 0.0 ? speedDarkToLight : speedLightToDark;

    // Exponential decay
    return previousLuminance + delta * (1.0 - exp2(-deltaTime * speed));
}

#endif
