// cbuffer TransformCBuf : register(b0)
// {
//     matrix model;
//     matrix modelView;
//     matrix modelViewProj;
// };
cbuffer ObjectTransform : register(b0)
{
    matrix model;
    matrix model_view;
    matrix model_view_proj;
    float4 tex_ts;
};
