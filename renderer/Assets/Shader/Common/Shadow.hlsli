#ifndef COMMON_SHADOW_
#define COMMON_SHADOW_

struct CascadeShadowMapInfo
{
    uint cascade_levels;
    float partion_size;
    float2 texel_size;
    int pcf_start;
    int pcf_end;
    float bias;
    float blend_band;
    float4 cascade_interval[2];
    matrix coord_trans[8];
};

int CalcCascadeIndex(in float3 vpos, in CascadeShadowMapInfo info)
{
    int index = 0;
    int cascade_count = info.cascade_levels;
    if (cascade_count > 1)
    {
        float4 vCurrentPixelDepth = vpos.z;
        float4 fComparison = (vCurrentPixelDepth > info.cascade_interval[0]);
        float4 fComparison2 = (vCurrentPixelDepth > info.cascade_interval[1]);
        float fIndex = dot(float4(cascade_count > 0, cascade_count > 1, cascade_count > 2, cascade_count > 3)
                            , fComparison) +
                        dot(float4(cascade_count > 4, cascade_count > 5, cascade_count > 6, cascade_count > 7)
                            , fComparison2);
        fIndex = min(fIndex, info.cascade_levels - 1);
        index = (int) fIndex;
    }
    return index;
}

//-------------------------------------------------------------------------------------------------
// Calculates the offset to use for sampling the shadow map, based on the surface normal
//-------------------------------------------------------------------------------------------------
//float3 GetShadowPosOffset(in float nDotL, in float3 normal)
//{
//    float2 shadowMapSize;
//    float numSlices;
//    ShadowMap.GetDimensions(shadowMapSize.x, shadowMapSize.y, numSlices);
//    float texelSize = 2.0f / shadowMapSize.x;
//    float nmlOffsetScale = saturate(1.0f - nDotL);
//    return texelSize * OffsetScale * nmlOffsetScale * normal;
//}

//    // Apply offset
//float3 offset = GetShadowPosOffset(nDotL, normal) / abs(CascadeScales[cascadeIdx].z);

//    // Project into shadow space
//float3 samplePos = positionWS + offset;
//float3 shadowPosition = mul(float4(samplePos, 1.0f), ShadowMatrix).xyz;

float4 CalcShadowCoord(in float3 wpos, in int cascade_index, in CascadeShadowMapInfo info)
{
    float4 shadow_coords = mul(float4(wpos, 1.0f), info.coord_trans[cascade_index]);
    shadow_coords.x *= info.partion_size;
    shadow_coords.x += (info.partion_size * cascade_index);
    return shadow_coords;
}

float CalculatePCFShadow(in float4 shadow_coord, 
    in CascadeShadowMapInfo info,
    in Texture2D shadow_tex,
    in SamplerComparisonState cmp_sampler,
    float bias)
{
    float shadow_level = 0.0f;
    // This loop could be unrolled, and texture immediate offsets could be used if the kernel size were fixed.
    // This would be performance improvment.
    for (int x = info.pcf_start; x < info.pcf_end; ++x)
    {
        for (int y = info.pcf_start; y < info.pcf_end; ++y)
        {
#ifdef GLACIER_REVERSE_Z
            float depth = shadow_coord.z + bias;
#else
            float depth = shadow_coord.z - bias;
#endif
            shadow_level += shadow_tex.SampleCmpLevelZero(cmp_sampler,
                float2(shadow_coord.x + (((float) x) * info.texel_size.x),
                    shadow_coord.y + (((float) y) * info.texel_size.y)),
                depth);
        }
    }
    
    float blur_size = (info.pcf_end - info.pcf_start);
    blur_size *= blur_size;
    
    shadow_level /= blur_size;
    return shadow_level;
}

void CalculateBlendFactorForInterval(
    in int cascade_index,
    in float vdepth,
    in CascadeShadowMapInfo info,
    in out float blend_band_location,
    out float blend_factor
    )
{
    float interval = ((float[8]) info.cascade_interval)[cascade_index];
    int pre_index = min(0, cascade_index - 1);
    vdepth -= ((float[8]) info.cascade_interval)[pre_index];
    interval -= ((float[8]) info.cascade_interval)[pre_index];
    
    // The current pixel's blend band location will be used to determine when we need to blend and by how much.
    blend_band_location = vdepth / interval;
    blend_band_location = 1.0f - blend_band_location;
    // The blend_factor is our location in the blend band.
    blend_factor = blend_band_location / info.blend_band;
}

float CalcShadowLevel(float3 light_dir, float3 normal, float3 vpos, float3 wpos, in CascadeShadowMapInfo shadow_info,
    in Texture2D shadow_tex,
    in SamplerComparisonState cmp_sampler)
{
    float3 L = normalize(-light_dir);
    float NdotL = max(dot(normal, L), 0);
    float bias = max(0.01f * (1.0 - NdotL), 0.005f);
    
    int cascade_index = CalcCascadeIndex(vpos, shadow_info);
    float4 shadow_coord = CalcShadowCoord(wpos, cascade_index, shadow_info);
    float shadow_level = CalculatePCFShadow(shadow_coord, shadow_info, shadow_tex, cmp_sampler, bias);
        
    int next_cascade_index = min(cascade_index + 1, shadow_info.cascade_levels - 1);
    if (next_cascade_index > cascade_index)
    {
        float blend_band_location = 1.0f;
        float blend_factor = 0.0f;
        CalculateBlendFactorForInterval(cascade_index, vpos.z, shadow_info,
                    blend_band_location, blend_factor);
        
        if (blend_band_location < shadow_info.blend_band)
        {
            int next_cascade_index = min(cascade_index + 1, shadow_info.cascade_levels - 1);
            shadow_coord = CalcShadowCoord(wpos, next_cascade_index, shadow_info);
            float blend_shadow_level = CalculatePCFShadow(shadow_coord, shadow_info, shadow_tex, cmp_sampler, bias);
            shadow_level = lerp(blend_shadow_level, shadow_level, blend_factor);
        }
    }
    
    return shadow_level;
}

#endif
