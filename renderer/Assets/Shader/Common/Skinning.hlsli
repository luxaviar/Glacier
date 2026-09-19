#ifndef COMMON_SKINNING_
#define COMMON_SKINNING_

#include "Common/BasicBuffer.hlsli"
#include "Common/Instance.hlsli"

//The skinning matrices of every skinned mesh of the frame live in one shared
//structured buffer (Render/Skinning/BoneMatrixPool) instead of a constant
//buffer per object, so a mesh only pays for the bones it has. _BoneOffset and
//_PrevBoneOffset of _PerObjectData say where the matrices of the object being
//drawn are; the matrices are in mesh space, so the vertex shader only has to
//weight them (see SkinnedMeshRenderer::UpdateBoneMatrices).
StructuredBuffer<float4x4> _BoneMatrices : register(t0, space1);

//Where the skinning matrices of the object (or of the instance of a batched
//draw) that is being drawn start in _BoneMatrices.
uint BoneSlot(uint instance_id, bool previous)
{
#ifdef GLACIER_INSTANCING
    uint4 slots = LoadInstanceData(instance_id).skin_offsets;
    return previous ? slots.y : slots.x;
#else
    return previous ? _PrevBoneOffset : _BoneOffset;
#endif
}

float4x4 BuildSkinMatrix(uint4 bone_indices, float4 bone_weights, uint instance_id, bool previous)
{
    uint base_index = BoneSlot(instance_id, previous);
    float4x4 skin = (float4x4)0;

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float weight = bone_weights[i];
        if (weight <= 0.0f) continue;

        skin += _BoneMatrices[base_index + bone_indices[i]] * weight;
    }

    return skin;
}

float3 SkinPosition(uint4 bone_indices, float4 bone_weights, float3 position, uint instance_id, bool previous)
{
    return mul(float4(position, 1.0f), BuildSkinMatrix(bone_indices, bone_weights, instance_id, previous)).xyz;
}

//normal and tangent are transformed with the same matrix, which is only exact
//for rigid bone transforms (no shear or non uniform bone scale)
float3 SkinDirection(uint4 bone_indices, float4 bone_weights, float3 direction, uint instance_id, bool previous)
{
    return mul(direction, (float3x3)BuildSkinMatrix(bone_indices, bone_weights, instance_id, previous));
}

#endif
