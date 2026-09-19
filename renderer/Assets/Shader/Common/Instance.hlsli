#ifndef COMMON_INSTANCE_
#define COMMON_INSTANCE_

#include "Common/BasicBuffer.hlsli"

//A batched draw cannot use _PerObjectData, which only carries the object that
//was bound last: every instance of the batch reads its own data from a
//structured buffer instead. _InstanceOffset of _PerObjectData says where the
//records of the batch start; the records of one frame are written by
//Render/Skinning/InstanceBuffer.
#ifdef GLACIER_INSTANCING

struct InstanceData
{
    float4x4 model;         //0
    float4x4 prev_model;    //64
    float4 tex_tile_scale;  //128
    //x: slot of the bones of this instance in _BoneMatrices, y: the slot of the
    //pose it was drawn with in the previous frame, see BoneMatrixPool
    uint4 skin_offsets;     //144 (160 bytes in total)
};

StructuredBuffer<InstanceData> _Instances : register(t1, space1);

InstanceData LoadInstanceData(uint instance_id)
{
    return _Instances[_InstanceOffset + instance_id];
}

#endif

//the object data of the object that is being drawn, which is the record of the
//instance for a batched draw and the constant buffer of the frame otherwise
float4x4 ObjectModel(uint instance_id)
{
#ifdef GLACIER_INSTANCING
    return LoadInstanceData(instance_id).model;
#else
    return _Model;
#endif
}

float4x4 ObjectPrevModel(uint instance_id)
{
#ifdef GLACIER_INSTANCING
    return LoadInstanceData(instance_id).prev_model;
#else
    return _PrevModel;
#endif
}

float4 ObjectTexTileScale(uint instance_id)
{
#ifdef GLACIER_INSTANCING
    return LoadInstanceData(instance_id).tex_tile_scale;
#else
    return _TextureTileScale;
#endif
}

float4x4 ObjectModelView(uint instance_id)
{
#ifdef GLACIER_INSTANCING
    return _View * LoadInstanceData(instance_id).model;
#else
    return _ModelView;
#endif
}

float4x4 ObjectModelViewProjection(uint instance_id)
{
#ifdef GLACIER_INSTANCING
    return _ViewProjection * LoadInstanceData(instance_id).model;
#else
    return _ModelViewProjection;
#endif
}

#endif
