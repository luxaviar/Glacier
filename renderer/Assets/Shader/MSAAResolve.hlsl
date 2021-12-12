#ifndef MSAASamples_
    #define MSAASamples_ 1
#endif

struct VSOut {
    float4 position : SV_Position;
    float2 uv : Texcoord;
};

VSOut main_vs(uint id : SV_VertexID)
{
   VSOut output;

   // Calculate the UV via the id
   output.uv = float2((id << 1) & 2, id & 2);

   // Convert the UV to the (-1, 1) to (3, -3) range for position
   output.position = float4(output.uv, 0, 1);
   output.position.x = output.position.x * 2 -1;
   output.position.y = output.position.y * -2 + 1;

   return output;
}

Texture2DMS<float4> color_buffer : register(t0);
Texture2DMS<float> depth_buffer : register(t1);

struct PSOut
{
    float4 color : SV_Target;
    float depth : SV_Depth;
};

PSOut main_ps(VSOut IN)
{
    PSOut output;
    output.depth = depth_buffer.Load(int2(IN.position.xy), 0);
    for(uint i = 0; i < MSAASamples_; i++)
    {
        output.color += color_buffer.Load(int2(IN.position.xy), i);
    }
    output.color /= MSAASamples_;

    return output;
}
