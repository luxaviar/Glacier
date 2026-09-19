#include "Common/BasicBuffer.hlsli"
#include "Common/Skinning.hlsli"
#include "Common/Instance.hlsli"

struct Output
{
    float3 viewPos : Position;
    float4 pos : SV_Position;
};

#ifdef GLACIER_SKINNING
struct Input
{
    float3 position : POSITION;
    float4 bone_weights : BlendWeight;
    uint4 bone_indices : BlendIndex;
};

Output main_vs(Input IN, uint instance_id : SV_InstanceID)
{
    float3 position = SkinPosition(IN.bone_indices, IN.bone_weights, IN.position, instance_id, false);

    Output output;
    output.pos = mul(float4(position, 1.0f), ObjectModelViewProjection(instance_id));
    output.viewPos = mul(float4(position, 1.0f), ObjectModelView(instance_id)).xyz;
    return output;
}
#else
Output main_vs(float3 pos : Position, uint instance_id : SV_InstanceID)
{
    Output output;
    output.pos = mul(float4(pos, 1.0f), ObjectModelViewProjection(instance_id));
    output.viewPos = mul(float4(pos, 1.0f), ObjectModelView(instance_id)).xyz;
    return output;
}
#endif

float4 main_ps(float3 viewPos : Position) : SV_TARGET
{
    return length(viewPos) * _CameraParams.w;
}
