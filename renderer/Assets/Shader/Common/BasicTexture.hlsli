#ifndef COMMON_BASIC_TEXTURE_
#define COMMON_BASIC_TEXTURE_

TextureCube SkyboxTextureCube_;
TextureCube RadianceTextureCube_;
TextureCube IrradianceTextureCube_;
Texture2D ShadowTexture_;
Texture2D BrdfLutTexture_;

Texture2D PostSourceTexture_;

Texture2D<float> DepthBuffer_;

#endif
