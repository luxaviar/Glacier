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

static const float PI= 3.14159265359;
static const uint SAMPLE_COUNT = 1024;

float RadicalInverse(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 Hammersley(uint i, float invN)
{
    return float2(float(i) * invN, RadicalInverse(i));
}

float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float a = roughness * roughness;

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    //Spherical to cartesian
    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    //Tangent space cartesian to world space
    float3 up = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);

    //Multiplaying halfway vector times implicit TBN matrix
    float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}


TextureCube tex : register(t0);
SamplerState tex_sam : register(s0);

cbuffer Roughness : register(b1)
{
    float roughness;
    float3 padding;
};

//Filtering down an incoming environment cubemap to different surface roughness levels
//the levels are essentially mipmaps of the cube
//the different mip levels are called when the shader is loaded on the application
float4 main_ps(float3 wpos : Position) : SV_TARGET
{
    float3 N = normalize(wpos);
    float3 R = N;
    float3 V = R;

    float total_weight = 0.0;
    float3 prefiltered_color = 0.0;

    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = Hammersley(i, 1.0f / SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, roughness);
        float3 L = normalize(2.0 * dot(V, H) * H - V);

        float nDotL = max(dot(N, L), 0.0);
        if (nDotL > 0.0)
        {
            prefiltered_color += tex.Sample(tex_sam, L).rgb * nDotL;
            total_weight += nDotL;
        }
    }

    prefiltered_color = prefiltered_color / total_weight;

    return float4(prefiltered_color, 1.0);
}
