struct Mat
{
    matrix ModelMatrix;
    matrix ModelViewMatrix;
    matrix InverseTransposeModelViewMatrix;
    matrix ModelViewProjectionMatrix;
};

ConstantBuffer<Mat> MatCB : register(b0);

struct VertexPositionNormalTexture
{
    float3 Position  : POSITION;
    float3 Normal    : NORMAL;
    float2 TexCoord  : TEXCOORD;
    float3 Tangent    : TANGENT;
};

struct VertexShaderOutput
{
    float4 PositionVS : POSITION;
    float3 NormalVS   : NORMAL;
    float2 TexCoord   : TEXCOORD;
    float4 Position   : SV_Position;
};

VertexShaderOutput main_vs(VertexPositionNormalTexture IN)
{
    VertexShaderOutput OUT;

    OUT.Position = mul( float4(IN.Position, 1.0f), MatCB.ModelViewProjectionMatrix);
    OUT.PositionVS = mul( float4(IN.Position, 1.0f), MatCB.ModelViewMatrix);
    OUT.NormalVS = mul(IN.Normal, (float3x3)MatCB.InverseTransposeModelViewMatrix);
    OUT.TexCoord = IN.TexCoord;

    return OUT;
}