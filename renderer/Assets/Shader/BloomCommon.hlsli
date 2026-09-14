#ifndef BLOOM_COMMON_
#define BLOOM_COMMON_

//shared by every bloom pass, the layout must match BloomParam in Bloom.h
cbuffer bloom_params {
    float4 _BloomThreshold;  //x: threshold, y: threshold - soft knee, z: soft knee * 2, w: 0.25 / soft knee
    float4 _BloomParams;     //x: scatter, y: intensity, zw: 1 / bloom texture size
    float4 _BloomTexelSize;  //xy: source texel size, zw: destination texel size
    float4 _BloomClamp;      //x: max brightness a single bloom texel can contribute
};

//threshold with a soft knee, bright pixels are ramped in instead of popping in
float3 PrefilterBright(float3 color) {
    float brightness = max(color.r, max(color.g, color.b));

    float soft = clamp(brightness - _BloomThreshold.y, 0.0, _BloomThreshold.z);
    soft = soft * soft * _BloomThreshold.w;

    float contribution = max(soft, brightness - _BloomThreshold.x) / max(brightness, 1e-5);
    return min(color * contribution, _BloomClamp.x);
}

#endif
