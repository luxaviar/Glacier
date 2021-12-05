#include "geometry.h"
#include <stdexcept>
#include "render/camera.h"
#include "Math/Util.h"

namespace glacier {
namespace render {
namespace geometry {

constexpr float SQRT2 = 1.41421356237309504880f;
constexpr float SQRT3 = 1.73205080756887729352f;
constexpr float SQRT6 = 2.44948974278317809820f;

// Helper computes a point on a unit circle, aligned to the x/z plane and centered on the origin.
inline Vec3f GetCircleVector(size_t i, size_t tessellation) noexcept
{
    float angle = float(i) * math::k2PI / float(tessellation);
    float dx, dz;

    math::FastSinCos(&dx, &dz, angle);

    return {dx, 0, dz};
}

// Helper for flipping winding of geometric primitives for LH vs. RH coords
inline void ReverseWinding(IndexCollection& indices, VertexCollection& vertices)
{
    assert((indices.size() % 3) == 0);
    for (auto it = indices.begin(); it != indices.end(); it += 3)
    {
        std::swap(*it, *(it + 2));
    }

    for (auto it = vertices.begin(); it != vertices.end(); ++it)
    {
        it->texcoord.x = (1.f - it->texcoord.x);
    }
}

// Helper for inverting normals of geometric primitives for 'inside' vs. 'outside' viewing
inline void InvertNormals(VertexCollection& vertices)
{
    for (auto it = vertices.begin(); it != vertices.end(); ++it)
    {
        it->normal.x = -it->normal.x;
        it->normal.y = -it->normal.y;
        it->normal.z = -it->normal.z;
    }
}

void CalcTangent(VertexCollection& vertices, IndexCollection& indices) {
    for (size_t i = 0; i < indices.size(); i += 3) {
        auto i0 = indices[i + 2];
        auto i1 = indices[i + 1];
        auto i2 = indices[i];

        auto& v0 = vertices[i0];
        auto& v1 = vertices[i1];
        auto& v2 = vertices[i2];

        Vec3f q1 = v1.position - v0.position;
        Vec3f q2 = v2.position - v0.position;

        Vec2f duv1 = v1.texcoord - v0.texcoord;
        Vec2f duv2 = v2.texcoord - v0.texcoord;
        float du1 = duv1.u;
        float dv1 = duv1.v;
        float du2 = duv2.u;
        float dv2 = duv2.v;
        float r = 1.0f / (du1 * dv2 - du2 * dv1);

        Vec3f tangent{
            r * (dv2 * q1.x - dv1 * q2.x),
            r * (dv2 * q1.y - dv1 * q2.y),
            r * (dv2 * q1.z - dv1 * q2.z) 
        };

        Vec3f bitangent{
            r * (du1 * q2.x - du2 * q1.x),
            r * (du1 * q2.y - du2 * q1.y),
            r * (du1 * q2.z - du2 * q1.z)
        };

        v0.tangent += tangent;
        v1.tangent += tangent;
        v2.tangent += tangent;

        v0.bitangent += bitangent;
        v1.bitangent += bitangent;
        v2.bitangent += bitangent;
    }

    for (int i = 0; i < vertices.size(); ++i) {
        auto& v = vertices[i];
        Vec3f normal = v.normal;
        Vec3f tangent = v.tangent;
        Vec3f bitangent = v.bitangent;

        tangent = (tangent - (normal.Dot(tangent) * normal)).Normalized();
        float c = normal.Cross(tangent).Dot(bitangent);
        if (c < 0.0f) {
            tangent *= -1.0f;
        }

        v.tangent = tangent;
    }
}

// Helper creates a triangle fan to close the end of a cylinder / cone
void CreateCylinderCap(VertexCollection& vertices, IndexCollection& indices,
    uint32_t tessellation, float height, float radius, bool isTop)
{
    // Create cap indices.
    for (uint32_t i = 0; i < tessellation - 2; i++) {
        uint32_t i1 = (i + 1) % tessellation;
        uint32_t i2 = (i + 2) % tessellation;

        if (isTop) {
            std::swap(i1, i2);
        }

        auto vbase = (uint32_t)vertices.size();
        indices.push_back(vbase);
        indices.push_back(vbase + i1);
        indices.push_back(vbase + i2);
    }

    // Which end of the cylinder is this?
    auto normal = Vec3f::up;
    auto textureScale = -Vec2f::half;

    if (!isTop) {
        normal = -normal;
        textureScale.x = -textureScale.x;
    }

    // Create cap vertices.
    for (size_t i = 0; i < tessellation; i++) {
        auto circleVector = GetCircleVector(i, tessellation);
        auto position = circleVector * radius + normal * height;
        auto textureCoordinate = Vec2f{ circleVector.x, circleVector.z} * textureScale + Vec2f::half;

        vertices.emplace_back(position, normal, textureCoordinate);
    }
}

void CreateCube(VertexCollection& vertices, IndexCollection& indices, const Vec3f& size, bool rhcoords, bool invertn) {
    vertices.clear();
    indices.clear();

    // A box has six faces, each one pointing in a different direction.
    constexpr int FaceCount = 6;

    static constexpr Vec3f faceNormals[FaceCount] =
    {
        {  0,  0,  1 },
        {  0,  0, -1 },
        {  1,  0,  0 },
        { -1,  0,  0 },
        {  0,  1,  0 },
        {  0, -1,  0 },
    };

    static constexpr Vec2f textureCoordinates[4] =
    {
        { 1, 0 },
        { 1, 1 },
        { 0, 1 },
        { 0, 0 },
    };

    auto tsize = size / 2.0f;

    // Create each face in turn.
    for (int i = 0; i < FaceCount; i++)
    {
        auto normal = faceNormals[i];

        // Get two vectors perpendicular both to the face normal and to each other.
        auto basis = (i >= 4) ? Vec3f::forward : Vec3f::up;

        auto side1 = normal.Cross(basis);
        auto side2 = normal.Cross(side1);

        // Six indices (two triangles) per face.
        auto vbase = (uint32_t)vertices.size();
        indices.push_back(vbase + 0);
        indices.push_back(vbase + 1);
        indices.push_back(vbase + 2);

        indices.push_back(vbase + 0);
        indices.push_back(vbase + 2);
        indices.push_back(vbase + 3);

        // Four vertices per face.
        // (normal - side1 - side2) * tsize // normal // t0
        vertices.emplace_back((normal - side1 - side2) * tsize, normal, textureCoordinates[0]);

        // (normal - side1 + side2) * tsize // normal // t1
        vertices.emplace_back((normal - side1 + side2) * tsize, normal, textureCoordinates[1]);

        // (normal + side1 + side2) * tsize // normal // t2
        vertices.emplace_back((normal + side1 + side2) * tsize, normal, textureCoordinates[2]);

        // (normal + side1 - side2) * tsize // normal // t3
        vertices.emplace_back((normal + side1 - side2) * tsize, normal, textureCoordinates[3]);
    }

    // Build RH above
    if (!rhcoords)
        ReverseWinding(indices, vertices);

    if (invertn)
        InvertNormals(vertices);

    CalcTangent(vertices, indices);
}

void CreateTetrahedron(VertexCollection& vertices, IndexCollection& indices, float size, bool rhcoords) {
    vertices.clear();
    indices.clear();

    static constexpr Vec3f verts[4] =
    {
        { 0.f, 0.f, 1.f },
        { 2.f * SQRT2 / 3.f, 0.f, -1.f / 3.f },
        { -SQRT2 / 3.f, SQRT6 / 3.f, -1.f / 3.f },
        { -SQRT2 / 3.f, -SQRT6 / 3.f, -1.f / 3.f }
    };

    static constexpr uint32_t faces[4 * 3] =
    {
        0, 1, 2,
        0, 2, 3,
        0, 3, 1,
        1, 3, 2,
    };

    for (size_t j = 0; j < _countof(faces); j += 3)
    {
        uint32_t v0 = faces[j];
        uint32_t v1 = faces[j + 1];
        uint32_t v2 = faces[j + 2];

        auto normal = (verts[v1] - verts[v0]).Cross(verts[v2] - verts[v0]);
        normal.Normalize();

        uint32_t base = (uint32_t)vertices.size();
        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 2);

        // Duplicate vertices to use face normals
        auto position = verts[v0] * size;
        vertices.emplace_back(position, normal, Vec2f::zero /* 0, 0 */);

        position = verts[v1] * size;
        vertices.emplace_back(position, normal, Vec2f::right /* 1, 0 */);

        position = verts[v2] * size;
        vertices.emplace_back(position, normal, Vec2f::up /* 0, 1 */);
    }

    // Built LH above
    if (rhcoords)
        ReverseWinding(indices, vertices);

    assert(vertices.size() == 4 * 3);
    assert(indices.size() == 4 * 3);

    CalcTangent(vertices, indices);
}

void CreateFrustumIndicator(VertexCollection& vertices, IndexCollection& indices) {
    vertices.clear();
    indices.clear();

    constexpr float x = 4.0f / 3.0f * 0.75f;
    constexpr float y = 1.0f * 0.75f;
    constexpr float z = -2.0f;
    constexpr float thalf = x * 0.5f;
    constexpr float tspace = y * 0.2f;

    vertices.emplace_back(Vec3f{ -x,y,0.0f });
    vertices.emplace_back(Vec3f{ x,y,0.0f });
    vertices.emplace_back(Vec3f{ x,-y,0.0f });
    vertices.emplace_back(Vec3f{ -x,-y,0.0f });
    vertices.emplace_back(Vec3f{ 0.0f,0.0f,z });
    vertices.emplace_back(Vec3f{ -thalf,y + tspace,0.0f });
    vertices.emplace_back(Vec3f{ thalf,y + tspace,0.0f });
    vertices.emplace_back(Vec3f{ 0.0f,y + tspace + thalf,0.0f });

    indices.push_back(0); indices.push_back(1);
    indices.push_back(1); indices.push_back(2);
    indices.push_back(2); indices.push_back(3);
    indices.push_back(3); indices.push_back(0);
    indices.push_back(0); indices.push_back(4);
    indices.push_back(1); indices.push_back(4);
    indices.push_back(2); indices.push_back(4);
    indices.push_back(3); indices.push_back(4);
    indices.push_back(5); indices.push_back(6);
    indices.push_back(6); indices.push_back(7);
    indices.push_back(7); indices.push_back(5);

    CalcTangent(vertices, indices);
}

void CreateSphere(VertexCollection& vertices, IndexCollection& indices, 
    float radius, uint16_t tessellation, bool rhcoords, bool invertn)
{
    vertices.clear();
    indices.clear();

    if (tessellation < 3)
        throw std::out_of_range("tesselation parameter out of range");

    uint16_t verticalSegments = tessellation;
    uint16_t horizontalSegments = tessellation * 2;

    //float radius = diameter / 2;

    // Create rings of vertices at progressively higher latitudes.
    for (size_t i = 0; i <= verticalSegments; i++)
    {
        float v = 1 - float(i) / float(verticalSegments);

        float latitude = (float(i) * math::kPI / float(verticalSegments)) - math::kPI / 2.0f;
        float dy = math::Sin(latitude);
        float dxz = math::Cos(latitude);

        // Create a single ring of vertices at this latitude.
        for (size_t j = 0; j <= horizontalSegments; j++)
        {
            float u = float(j) / float(horizontalSegments);

            float longitude = float(j) * math::kPI * 2.0f / float(horizontalSegments);
            float dx, dz;
            math::FastSinCos(&dx, &dz, longitude);

            dx *= dxz;
            dz *= dxz;

            Vec3f normal = { dx, dy, dz };
            Vec2f textureCoordinate = { u, v };

            vertices.emplace_back(normal *radius, normal, textureCoordinate);
        }
    }

    // Fill the index buffer with triangles joining each pair of latitude rings.
    uint32_t stride = horizontalSegments + 1;

    for (uint32_t i = 0; i < verticalSegments; i++)
    {
        for (uint32_t j = 0; j <= horizontalSegments; j++)
        {
            uint32_t nextI = i + 1;
            uint32_t nextJ = (j + 1) % stride;

            indices.push_back(i * stride + j);
            indices.push_back(nextI * stride + j);
            indices.push_back(i * stride + nextJ);

            indices.push_back(i * stride + nextJ);
            indices.push_back(nextI * stride + j);
            indices.push_back(nextI * stride + nextJ);
        }
    }

    // Build RH above
    if (!rhcoords)
        ReverseWinding(indices, vertices);

    if (invertn)
        InvertNormals(vertices);

    CalcTangent(vertices, indices);
}

void CreateCylinder(VertexCollection& vertices, IndexCollection& indices,
    float height, float radius, uint16_t tessellation, bool rhcoords, bool invertn)
{
    if (tessellation < 3)
        throw std::out_of_range("tesselation parameter out of range");

    float half_height = height * 0.5f;

    Vec3f topOffset{ 0, half_height, 0 };

    uint32_t stride = tessellation + 1;

    // Create a ring of triangles around the outside of the cylinder.
    for (uint32_t i = 0; i <= tessellation; i++) {
        auto normal = GetCircleVector(i, tessellation);
        auto sideOffset = normal * radius;

        float u = float(i) / float(tessellation);

        auto textureCoordinate = Vec2f(u);

        vertices.emplace_back(sideOffset + topOffset, normal, textureCoordinate);
        vertices.emplace_back(sideOffset - topOffset, normal, textureCoordinate + Vec2f::up);

        indices.push_back(i * 2);
        indices.push_back((i * 2 + 2) % (stride * 2));
        indices.push_back(i * 2 + 1);

        indices.push_back(i * 2 + 1);
        indices.push_back((i * 2 + 2) % (stride * 2));
        indices.push_back((i * 2 + 3) % (stride * 2));
    }

    // Create flat triangle fan caps to seal the top and bottom.
    CreateCylinderCap(vertices, indices, tessellation, half_height, radius, true);
    CreateCylinderCap(vertices, indices, tessellation, half_height, radius, false);

    // Build RH above
    if (!rhcoords)
        ReverseWinding(indices, vertices);

    if (invertn)
        InvertNormals(vertices);

    CalcTangent(vertices, indices);
}

//https://github.com/godotengine/godot/blob/master/scene/resources/primitive_meshes.cpp
void CreateCapsule(VertexCollection& vertices, IndexCollection& indices,
    float height, float radius, uint16_t tessellation, bool rhcoords, bool invertn)
{
    constexpr float onethird = 1.0f / 3.0f;
    constexpr float twothirds = 2.0f / 3.0f;
    constexpr int rings = 8;

    // note, this has been aligned with our collision shape but I've left the descriptions as top/middle/bottom
    int point = 0;
    int radial_segments = tessellation;
    float mid_height = height;

    /* top hemisphere */
    int thisrow = 0;
    int prevrow = 0;

    for (int j = 0; j <= (rings + 1); j++) {
        float v = j / (rings + 1.0f);
        float w, y;
        math::FastSinCos(&w, &y, 0.5f * math::kPI * v);
        y *= radius;

        for (int i = 0; i <= radial_segments; i++) {
            float u = (float)i / radial_segments;
            float x, z;
            math::FastSinCos(&x, &z, u * math::k2PI);
            x = -x;

            auto p = Vec3f(x * radius * w, y, -z * radius * w);
            vertices.emplace_back(p + Vec3f(0.0f, 0.5f * mid_height, 0.0f), p.Normalized(), Vec2f(u, v * onethird));

            point++;

            if (i > 0 && j > 0) {
                indices.push_back(prevrow + i - 1);
                indices.push_back(prevrow + i);
                indices.push_back(thisrow + i - 1);

                indices.push_back(prevrow + i);
                indices.push_back(thisrow + i);
                indices.push_back(thisrow + i - 1);
            };
        };

        prevrow = thisrow;
        thisrow = point;
    };

    /* cylinder */
    thisrow = point;
    prevrow = 0;
    for (int j = 0; j <= (rings + 1); j++) {
        float v = j / (rings + 1.0f);
        float y = mid_height * v;
        y = (mid_height * 0.5f) - y;

        for (int i = 0; i <= radial_segments; i++) {
            float u = (float)i / radial_segments;
            float x, z;
            math::FastSinCos(&x, &z, u * math::k2PI);
            x = -x;

            auto p = Vec3f(x * radius, y, -z * radius);
            vertices.emplace_back(p, Vec3f(x, 0.0f, -z), Vec2f(u, onethird + (v * onethird)));
            point++;

            if (i > 0 && j > 0) {
                indices.push_back(prevrow + i - 1);
                indices.push_back(prevrow + i);
                indices.push_back(thisrow + i - 1);

                indices.push_back(prevrow + i);
                indices.push_back(thisrow + i);
                indices.push_back(thisrow + i - 1);
            };
        };

        prevrow = thisrow;
        thisrow = point;
    };

    /* bottom hemisphere */
    thisrow = point;
    prevrow = 0;
    for (int j = 0; j <= (rings + 1); j++) {
        float v = j / (rings + 1.0f) + 1.0f;
        float w, y;
        math::FastSinCos(&w, &y, 0.5f * math::kPI * v);
        y *= radius;

        for (int i = 0; i <= radial_segments; i++) {
            float u = (float)i / radial_segments;
            float x, z;
            math::FastSinCos(&x, &z, u * math::k2PI);
            x = -x;

            auto p = Vec3f(x * radius * w, y, -z * radius * w);
            vertices.emplace_back(p + Vec3f(0.0f, -0.5f * mid_height, 0.0f), p.Normalized(), Vec2f(u, twothirds + ((v - 1.0f) * onethird)));
            point++;

            if (i > 0 && j > 0) {
                indices.push_back(prevrow + i - 1);
                indices.push_back(prevrow + i);
                indices.push_back(thisrow + i - 1);

                indices.push_back(prevrow + i);
                indices.push_back(thisrow + i);
                indices.push_back(thisrow + i - 1);
            };
        };

        prevrow = thisrow;
        thisrow = point;
    };

    // Build RH above
    if (!rhcoords)
        ReverseWinding(indices, vertices);

    if (invertn)
        InvertNormals(vertices);

    CalcTangent(vertices, indices);
}

void CreateFrustum(VertexCollection& vertices, IndexCollection& indices,
    float width, float height, float nearZ, float farZ) {
    vertices.clear();
    indices.clear();

    const float zRatio = farZ / nearZ;
    const float nearX = width / 2.0f;
    const float nearY = height / 2.0f;
    const float farX = nearX * zRatio;
    const float farY = nearY * zRatio;

    vertices.emplace_back(Vec3f{ -nearX,nearY,nearZ });
    vertices.emplace_back(Vec3f{ nearX,nearY,nearZ });
    vertices.emplace_back(Vec3f{ nearX,-nearY,nearZ });
    vertices.emplace_back(Vec3f{ -nearX,-nearY,nearZ });
    vertices.emplace_back(Vec3f{ -farX,farY,farZ });
    vertices.emplace_back(Vec3f{ farX,farY,farZ });
    vertices.emplace_back(Vec3f{ farX,-farY,farZ });
    vertices.emplace_back(Vec3f{ -farX,-farY,farZ });

    indices.push_back(0); indices.push_back(1);
    indices.push_back(1); indices.push_back(2);
    indices.push_back(2); indices.push_back(3);
    indices.push_back(3); indices.push_back(0);
    indices.push_back(4); indices.push_back(5);
    indices.push_back(5); indices.push_back(6);
    indices.push_back(6); indices.push_back(7);
    indices.push_back(7); indices.push_back(4);
    indices.push_back(0); indices.push_back(4);
    indices.push_back(1); indices.push_back(5);
    indices.push_back(2); indices.push_back(6);
    indices.push_back(3); indices.push_back(7);

    CalcTangent(vertices, indices);
}

void CreateFrustum(VertexCollection& vertices, IndexCollection& indices, const Camera& camera) {
    auto htox = tan(camera.fov() / 2.0f);
    auto near_h = htox * camera.nearz() * 2.0f;
    auto near_w = camera.aspect() * near_h;

    CreateFrustum(vertices, indices, near_w, near_h, camera.nearz(), camera.farz());

    for (auto& vertex : vertices) {
        vertex.position = camera.rotation() * vertex.position + camera.position();
    }
}

void CreateQuad(VertexCollection& vertices, IndexCollection& indices) {
    vertices.clear();
    indices.clear();

    vertices.emplace_back(Vec3f{ -1,1, 0});
    vertices.emplace_back(Vec3f{ 1,1, 0 });
    vertices.emplace_back(Vec3f{ -1,-1, 0 });
    vertices.emplace_back(Vec3f{ 1,-1, 0 });

    indices.push_back(0); indices.push_back(1);
    indices.push_back(2); indices.push_back(1);
    indices.push_back(3); indices.push_back(2);

    CalcTangent(vertices, indices);
}

void CreateArcPoint(Vec3f* vertices, int count, const Vec3f& center, const Vec3f& normal, 
    const Vec3f& from, float angle, float radius) {
    assert(count > 1);
    assert(radius > 0);
    assert(angle != 0);

    auto rot = Quaternion::AngleAxis(angle / (count - 1), normal);
    Vec3f tangent = from * radius;
    for (int i = 0; i < count; ++i) {
        vertices[i] = center + tangent;
        tangent = rot * tangent;
    }
}

void FetchFrustumCorners(Vec3f corners[(int)FrustumCorner::kCount], CameraType type, 
    float fov_degree_or_width, float aspect_or_height, float n, float f) {
    assert(n < f);

    if (type == CameraType::kPersp) {
        assert(n >= 0);
        auto htox = tan(fov_degree_or_width * math::kDeg2Rad * 0.5f);
        auto near_h = htox * n;
        auto far_h = htox * f;

        auto near_w = aspect_or_height * near_h;
        auto far_w = aspect_or_height * far_h;

        corners[(int)FrustumCorner::kNearBottomLeft] = { -near_w, -near_h, n };
        corners[(int)FrustumCorner::kNearTopLeft] = { -near_w, near_h, n };
        corners[(int)FrustumCorner::kNearTopRight] = { near_w, near_h, n };
        corners[(int)FrustumCorner::kNearBottomRight] = { near_w, -near_h, n };

        corners[(int)FrustumCorner::kFarBottomLeft] = { -far_w, -far_h, f };
        corners[(int)FrustumCorner::kFarTopLeft] = { -far_w, far_h, f };
        corners[(int)FrustumCorner::kFarTopRight] = { far_w, far_h, f };
        corners[(int)FrustumCorner::kFarBottomRight] = { far_w, -far_h, f };
    }
    else if (type == CameraType::kOrtho) {
        auto w = fov_degree_or_width / 2.0f;
        auto h = aspect_or_height / 2.0f;

        corners[(int)FrustumCorner::kNearBottomLeft] = { -w, -h, n };
        corners[(int)FrustumCorner::kNearTopLeft] = { -w, h, n };
        corners[(int)FrustumCorner::kNearTopRight] = { w, h, n };
        corners[(int)FrustumCorner::kNearBottomRight] = { w, -h, n };

        corners[(int)FrustumCorner::kFarBottomLeft] = { -w, -h, f };
        corners[(int)FrustumCorner::kFarTopLeft] = { -w, h, f };
        corners[(int)FrustumCorner::kFarTopRight] = { w, h, f };
        corners[(int)FrustumCorner::kFarBottomRight] = { w, -h, f };
    }
}

void FetchFrustumPlanes(Plane planes[(int)FrustumPlane::kCount], const Vec3f corners[(int)FrustumCorner::kCount]) {
    planes[(int)FrustumPlane::kNear] = Plane(corners[(int)FrustumCorner::kNearTopRight],
        corners[(int)FrustumCorner::kNearTopLeft], corners[(int)FrustumCorner::kNearBottomLeft]);

    planes[(int)FrustumPlane::kFar] = Plane(corners[(int)FrustumCorner::kFarBottomLeft],
        corners[(int)FrustumCorner::kFarTopLeft], corners[(int)FrustumCorner::kFarTopRight]);

    planes[(int)FrustumPlane::kLeft] = Plane(corners[(int)FrustumCorner::kNearTopLeft],
        corners[(int)FrustumCorner::kFarTopLeft], corners[(int)FrustumCorner::kFarBottomLeft]);

    planes[(int)FrustumPlane::kRight] = Plane(corners[(int)FrustumCorner::kFarBottomRight],
        corners[(int)FrustumCorner::kFarTopRight], corners[(int)FrustumCorner::kNearTopRight]);

    planes[(int)FrustumPlane::kBottom] = Plane(corners[(int)FrustumCorner::kNearBottomLeft],
        corners[(int)FrustumCorner::kFarBottomLeft], corners[(int)FrustumCorner::kFarBottomRight]);

    planes[(int)FrustumPlane::kTop] = Plane(corners[(int)FrustumCorner::kFarTopRight],
        corners[(int)FrustumCorner::kFarTopLeft], corners[(int)FrustumCorner::kNearTopLeft]);
}

}
}
}