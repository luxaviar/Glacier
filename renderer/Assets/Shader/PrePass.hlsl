#include "Common/BasicBuffer.hlsli"

struct Output
{
    // float3 viewPos : Position;
    float4 pos : SV_Position;
};

Output main_vs(float3 pos : Position)
{
    Output output;
    output.pos = mul(float4(pos, 1.0f), _ModelViewProjection);
    // output.viewPos = mul(float4(pos, 1.0f), _ModelView).xyz;
    return output;
}

// float4 main_ps(float3 viewPos : Position) : SV_TARGET
// {
//     return length(viewPos) * _CameraParams.w;
// }
