struct ModelViewProjection
{
    matrix MVP;
};

ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);

struct VertexPosColor
{
    float3 Position : POSITION;
    float3 Color    : COLOR;
};

struct VertexShaderOutput
{
    float4 Color    : COLOR;
    float4 Position : SV_Position;
};

VertexShaderOutput main_vs(VertexPosColor IN)
{
    VertexShaderOutput OUT;

    // OUT.Position = mul(ModelViewProjectionCB.MVP, float4(IN.Position, 1.0f));
    OUT.Position = mul(float4(IN.Position, 1.0f), ModelViewProjectionCB.MVP);
    OUT.Color = float4(IN.Color, 1.0f);

    return OUT;
}

struct PixelShaderInput
{
    float4 Color : COLOR;
};

float4 main_ps( PixelShaderInput IN ) : SV_Target
{
    // Return gamma corrected color.
    return pow( abs( IN.Color ), 1.0f / 2.2f );
    // return IN.Color;
}
