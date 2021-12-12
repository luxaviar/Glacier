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

Texture2D tex : register(t0);
SamplerState tex_sam : register(s0);
