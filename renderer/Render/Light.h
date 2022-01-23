#pragma once
#include "render/base/gfxdriver.h"
#include "channels.h"
#include "Math/Vec3.h"
#include "camera.h"
#include "core/component.h"
#include "common/color.h"

namespace glacier {
namespace render {

class LightManager;

enum class LightType : uint32_t {
    kDirectinal = 0,
    kPoint = 1,
    kSpot = 2
};

struct LightData {
    Vec3f position; //Directional: direction, Point: position
    LightType type;
    Vec3f view_position;
    float intensity;
    Color color; //rgba
    Matrix4x4 shadow_trans; //Directional: view project, Point: translate
    float range; //for point light
    uint32_t enable = 0;
    uint32_t shadow_enable = 0;
    float padding;
    //TODO: shadow toggle
};

class Light : public Component {
public:
    constexpr static float kConstantFactor = 1.0f;
    constexpr static float kQuadraticFactor = 25.0f;
    constexpr static int kShadowMapWidth = 1024;

    friend class LightManager;

    Light(LightType type, const Color& color, float intensity);

    void OnEnable() override;
    void OnDisable() override;

    virtual void Update(GfxDriver* gfx) noexcept = 0;

    bool HasShadow() const { return data_.shadow_enable; }
    void EnableShadow() { data_.shadow_enable = true; }
    void DisableShadow() { data_.shadow_enable = false; }

    LightType type() const { return data_.type; }

    Vec3f direction() const { return data_.position; }
    Vec3f position() const { return data_.position; }

    void SetColor(const Color& color);
    float LuminessAtPoint(const Vec3f& pos);
    float AttenuateApprox(float dist_sq) const;

    void DrawInspector() override;

protected:
    LightData data_;
    Color color_; //gamma color
};

class DirectionalLight : public Light {
public:
    DirectionalLight(const Color& color, float intensity);

    void Update(GfxDriver* gfx) noexcept override;
};

class PointLight : public Light {
public:
    PointLight(float range, const Color& color, float intensity);
    
    void Update(GfxDriver* gfx) noexcept override;
};

class OldPointLight
{
public:
    OldPointLight( GfxDriver& gfx, Vec3f pos = { 10.0f,9.0f,2.5f },float radius = 0.5f );

    void Bind(const Matrix4x4& view) const noexcept;

    Vec3f pos() const { return light_data_.pos; }

private:
    struct PointLightCBuf
    {
        alignas(16) Vec3f pos;
        alignas(16) Vec3f ambient;
        alignas(16) Vec3f diffuseColor;
        float diffuseIntensity;
        float attConst;
        float attLin;
        float attQuad;
    };
private:
    PointLightCBuf light_data_;
    std::shared_ptr<ConstantBuffer> light_cbuffer_;
};

}
}
