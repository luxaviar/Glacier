#ifndef COMMON_SKINNING_
#define COMMON_SKINNING_

//must match kMaxBones in Render/Base/Renderable.h
#ifndef MAX_BONES
#define MAX_BONES 128
#endif

cbuffer _BoneData
{
    //object space skinning matrices: inverse(mesh_world) * bone_world * inverse_bind
    float4x4 _BoneMatrices[MAX_BONES];
    //same for the previous frame, used to build the velocity buffer
    float4x4 _PrevBoneMatrices[MAX_BONES];
};

float4x4 BuildSkinMatrix(uint4 bone_indices, float4 bone_weights, bool previous)
{
    float4x4 skin = (float4x4)0;

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float weight = bone_weights[i];
        if (weight <= 0.0f) continue;

        skin += (previous ? _PrevBoneMatrices[bone_indices[i]] : _BoneMatrices[bone_indices[i]]) * weight;
    }

    return skin;
}

float3 SkinPosition(uint4 bone_indices, float4 bone_weights, float3 position, bool previous)
{
    return mul(float4(position, 1.0f), BuildSkinMatrix(bone_indices, bone_weights, previous)).xyz;
}

//normal and tangent are transformed with the same matrix, which is only exact
//for rigid bone transforms (no shear or non uniform bone scale)
float3 SkinDirection(uint4 bone_indices, float4 bone_weights, float3 direction, bool previous)
{
    return mul(direction, (float3x3)BuildSkinMatrix(bone_indices, bone_weights, previous));
}

#endif
