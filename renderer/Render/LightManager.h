#pragma once

#include <string>
#include <vector>
#include "Math/Vec3.h"
#include "Common/Singleton.h"
#include "render/base/gfxdriver.h"
#include "light.h"
#include "render/material.h"

namespace glacier {
namespace render {

class Camera;
class Renderer;
class Renderable;
class Material;
class MaterialTemplate;

class LightManager : public Singleton<LightManager> {
public:
    constexpr static int kMaxLightNum = 4;

    void Add(Light* light);
    void Remove(Light* light);

    void Setup(GfxDriver* gfx, Renderer* renderer);
    void AddSkyboxPass(Renderer* renderer);

    void SortLight(const Vec3f& pos);
    //Get main directional light
    DirectionalLight* GetMainLight();

    void SetupMaterial(MaterialTemplate* mat);
    void SetupMaterial(Material* mat);
    void Update();

    void Clear();

    void TestGenerateIrradiance();

private:
    void GenerateSkybox();
    void GenerateBrdfLut(Renderer* renderer);
    void GenerateIrradiance();
    void GenerateRadiance();

    std::vector<Light*> lights_;
    struct LightList {
        LightData lights[kMaxLightNum];
        float radiance_max_load = 5.0f;
        float padding[3];
    };

    GfxDriver* gfx_;
    LightList light_data_;
    std::shared_ptr<ConstantBuffer> light_cbuffer_;

    std::unique_ptr<Material> skybox_material_;
    std::shared_ptr<ConstantBuffer> skybox_matrix_;
    Renderable* skybox_cube_ = nullptr;

    std::shared_ptr<Texture> skybox_texture_;
    std::shared_ptr<Texture> radiance_;
    std::shared_ptr<Texture> irradiance_;
    std::shared_ptr<Texture> test_irradiance_;
    std::shared_ptr<Texture> brdf_lut_;
};

}
}
