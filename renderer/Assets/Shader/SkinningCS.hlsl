/**
 * Compute skinning: deforms the bind pose of a skinned mesh into the vertices
 * the passes draw, once per mesh and per frame, so a mesh that several passes
 * draw is skinned once instead of in the vertex shader of every pass.
 *
 * The skinned vertex and the position it had in the frame before are written
 * next to each other, which is what the velocity buffer is built from.
 *
 * The buffers are raw byte buffers because the attributes of a vertex sit at
 * the offsets of Mesh::kSkinnedLayout and Mesh::kSkinnedVertexLayout, which are
 * not all 16 byte aligned.
 */

#define SKINNING_THREADS 64

//must match Mesh::kSkinnedLayout
#define BIND_POSE_STRIDE 76
#define BIND_POSITION 0
#define BIND_NORMAL 12
#define BIND_TEXCOORD 24
#define BIND_TANGENT 32
#define BIND_WEIGHTS 44
#define BIND_INDICES 60

//must match Mesh::kSkinnedVertexLayout
#define SKINNED_VERTEX_STRIDE 56
#define SKINNED_POSITION 0
#define SKINNED_NORMAL 12
#define SKINNED_TEXCOORD 24
#define SKINNED_TANGENT 32
#define SKINNED_PREV_POSITION 44

//must match GpuSkinning::SkinParams
cbuffer _SkinningParams : register(b0)
{
    //vertices of the mesh
    uint _VertexCount;
    //region of this mesh in the pool of skinned vertices, in vertices
    uint _VertexOffset;
    //slot of the pose of this frame and of the one before it in _BoneMatrices
    uint _BoneOffset;
    uint _PrevBoneOffset;
};

//the bind pose of the mesh, which is Mesh::vertex_buffer
ByteAddressBuffer _BindPoseVertices : register(t0);
//the region of the pool the skinned vertices go to
RWByteAddressBuffer _SkinnedVertices : register(u0);
//the skinning matrices of the frame, see BoneMatrixPool
StructuredBuffer<float4x4> _BoneMatrices : register(t0, space1);

struct ComputeShaderInput
{
    uint3 GroupID           : SV_GroupID;
    uint3 GroupThreadID     : SV_GroupThreadID;
    uint3 DispatchThreadID  : SV_DispatchThreadID;
    uint GroupIndex         : SV_GroupIndex;
};

float2 LoadFloat2(ByteAddressBuffer buffer, uint offset)
{
    return float2(asfloat(buffer.Load(offset)), asfloat(buffer.Load(offset + 4)));
}

float3 LoadFloat3(ByteAddressBuffer buffer, uint offset)
{
    return float3(asfloat(buffer.Load(offset)), asfloat(buffer.Load(offset + 4)), asfloat(buffer.Load(offset + 8)));
}

float4 LoadFloat4(ByteAddressBuffer buffer, uint offset)
{
    return float4(asfloat(buffer.Load(offset)), asfloat(buffer.Load(offset + 4)),
        asfloat(buffer.Load(offset + 8)), asfloat(buffer.Load(offset + 12)));
}

uint4 LoadUInt4(ByteAddressBuffer buffer, uint offset)
{
    return uint4(buffer.Load(offset), buffer.Load(offset + 4), buffer.Load(offset + 8), buffer.Load(offset + 12));
}

void StoreFloat2(RWByteAddressBuffer buffer, uint offset, float2 value)
{
    buffer.Store(offset, asuint(value.x));
    buffer.Store(offset + 4, asuint(value.y));
}

void StoreFloat3(RWByteAddressBuffer buffer, uint offset, float3 value)
{
    buffer.Store(offset, asuint(value.x));
    buffer.Store(offset + 4, asuint(value.y));
    buffer.Store(offset + 8, asuint(value.z));
}

//the matrix that takes a vertex from the bind pose of the mesh into the pose
//that starts at 'base_index' in the palette
float4x4 BuildSkinMatrix(uint4 bone_indices, float4 bone_weights, uint base_index)
{
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

[numthreads(SKINNING_THREADS, 1, 1)]
void main_cs(ComputeShaderInput IN)
{
    uint vertex = IN.DispatchThreadID.x;
    if (vertex >= _VertexCount) return;

    uint bind_pose = vertex * BIND_POSE_STRIDE;

    float3 position = LoadFloat3(_BindPoseVertices, bind_pose + BIND_POSITION);
    float3 normal = LoadFloat3(_BindPoseVertices, bind_pose + BIND_NORMAL);
    float2 texcoord = LoadFloat2(_BindPoseVertices, bind_pose + BIND_TEXCOORD);
    float3 tangent = LoadFloat3(_BindPoseVertices, bind_pose + BIND_TANGENT);
    float4 weights = LoadFloat4(_BindPoseVertices, bind_pose + BIND_WEIGHTS);
    uint4 indices = LoadUInt4(_BindPoseVertices, bind_pose + BIND_INDICES);

    float4x4 skin = BuildSkinMatrix(indices, weights, _BoneOffset);
    float4x4 prev_skin = BuildSkinMatrix(indices, weights, _PrevBoneOffset);

    uint skinned = (_VertexOffset + vertex) * SKINNED_VERTEX_STRIDE;

    StoreFloat3(_SkinnedVertices, skinned + SKINNED_POSITION, mul(float4(position, 1.0f), skin).xyz);
    StoreFloat3(_SkinnedVertices, skinned + SKINNED_NORMAL, mul(normal, (float3x3)skin));
    StoreFloat2(_SkinnedVertices, skinned + SKINNED_TEXCOORD, texcoord);
    StoreFloat3(_SkinnedVertices, skinned + SKINNED_TANGENT, mul(tangent, (float3x3)skin));
    StoreFloat3(_SkinnedVertices, skinned + SKINNED_PREV_POSITION, mul(float4(position, 1.0f), prev_skin).xyz);
}
