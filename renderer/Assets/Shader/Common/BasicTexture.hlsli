#ifndef COMMON_BASIC_TEXTURE_
#define COMMON_BASIC_TEXTURE_

TextureCube _SkyboxTextureCube;
TextureCube _RadianceTextureCube;
TextureCube _IrradianceTextureCube;

Texture2D _ShadowTexture;
Texture2D _BrdfLutTexture;

Texture2D<float4> _PostSourceTexture;

Texture2D<float> _DepthBuffer;

#endif
