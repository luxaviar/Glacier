cbuffer vp_matrix : register(b1)
{
    matrix vp;
};

struct VSOut
{
    float3 wpos : Position;
    float4 pos : SV_Position;
};

VSOut main_vs(float3 pos : Position)
{
    VSOut vso;
    vso.wpos = pos;
    vso.pos = mul(float4(pos, 1.0f), vp);
    return vso;
}

TextureCube tex : register(t0);
SamplerState sam : register(s0);

static const float PI= 3.14159265359;

float4 main_ps(float3 wpos : Position) : SV_TARGET
{
    //We use the world space vector as a normal to the surface since in a cubemap it will
    //be interpolated over every face and make a unit sphere
    float3 normal = normalize(wpos);

    float3 irradiance = 0.0f;

    //Creating basis vectors
    float3 up    = float3(0.0, 1.0, 0.0);
    float3 right = cross(up, normal);
    up = cross(normal, right);

    float delta = 0.025;
    float nSamples = 0.0;
    for(float phi = 0.0; phi < 2.0 * PI; phi +=delta){
        for(float theta = 0.0; theta < 0.5 * PI; theta +=delta){
            //Spherical to cartesian
            float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));

            //Tangent space to world space
            float3 sampleDir = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;

            irradiance += tex.Sample(sam, sampleDir).rgb * cos(theta) * sin(theta);

            nSamples++;
        }
    }

    irradiance = PI * irradiance * (1.0f / nSamples);
    return float4(irradiance, 1.0);
}
