struct VSOut
{
    float2 uv : Texcoord;
    float4 pos : SV_Position;
};

VSOut main_vs(float3 pos : Position)
{
    VSOut vso;
    vso.pos = float4(pos.x, pos.y, 0.0f, 1.0f);
    vso.uv = float2((pos.x + 1) / 2.0f, -(pos.y - 1) / 2.0f);
    return vso;
}

Texture2D tex;
SamplerState linear_sampler;

cbuffer Kernel
{
    uint nTaps;
    //float coefficients[15];
    float4 coefficients_packed[4];
}

cbuffer Control
{
    bool horizontal;
}

static float coefficients[16] = (float[16])coefficients_packed;

float4 main_ps(float2 uv : Texcoord) : SV_Target
{
    float width, height;
    tex.GetDimensions(width, height);
    float dx, dy;
    if (horizontal)
    {
        dx = 1.0f / width;
        dy = 0.0f;
    }
    else
    {
        dx = 0.0f;
        dy = 1.0f / height;
    }
    const int r = nTaps / 2;
    
    float accAlpha = 0.0f;
    float3 maxColor = float3(0.0f, 0.0f, 0.0f);
    
    [unroll(128)]
    for (int i = -r; i <= r; i++)
    {
        const float2 tc = uv + float2(dx * i, dy * i);
        const float4 s = tex.Sample(linear_sampler, tc).rgba;
        const float coef = coefficients[i + r];
        accAlpha += s.a * coef;
        maxColor = max(s.rgb, maxColor);
    }
    return float4(maxColor, accAlpha);
}