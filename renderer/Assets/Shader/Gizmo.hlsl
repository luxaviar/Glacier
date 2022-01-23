struct GizmoOut
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
};

cbuffer mvp_matrix : register(b0)
{
    matrix mvp;
};

GizmoOut main_vs(float3 pos : POSITION, float4 color : COLOR) //, float3 color : COLOR, float2 coord : TEXCOORD) : SV_Position
{
    GizmoOut go;
    go.position = mul( float4(pos,1.0f),mvp );
    go.color = color.rgb;
    return go;
    //return mul(float4(pos, 1.0f), mvp);
}

cbuffer line_color : register(b0)
{
   float4 materialColor;
};

float4 main_ps(GizmoOut go) : SV_Target
{
    return float4(go.color, materialColor.a);
    //return materialColor;
}
