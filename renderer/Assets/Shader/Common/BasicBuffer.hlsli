#ifndef COMMON_BASIC_BUFFER_
#define COMMON_BASIC_BUFFER_

cbuffer _PerFrameData {
    matrix _View;
    matrix _InverseView;
    matrix _Projection;
    matrix _InverseProjection;
    matrix _UnjitteredInverseProjection;
    matrix _ViewProjection;
    matrix _UnjitteredViewProjection;
    matrix _InverseViewProjection;
    matrix _UnjitteredInverseViewProjection; 
    matrix _PrevViewProjection; //unjitered
    float4 _ScreenParam; //width, height, 1/width, 1/height
    float4 _CameraParams; //nearz, farz, 1/nearz, 1/farz
    float4 _ZBufferParams; // 1-far/near, 1 + far/near, x/far, y/far
    float _DeltaTime;
};

cbuffer _PerObjectData
{
    matrix _Model;
    matrix _ModelView;
    matrix _ModelViewProjection;
    matrix _PrevModel;
    float4 _TextureTileScale;
};

#endif
