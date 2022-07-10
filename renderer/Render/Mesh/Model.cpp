#include "model.h"
#include "Render/Graph/PassNode.h"
#include "Common/Util.h"
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags
#include "Core/GameObject.h"
#include "Common/Log.h"
#include <assimp/GltfMaterial.h>
#include "App.h"
#include "Render/Renderer.h"

namespace glacier {
namespace render {

#define AI_MATKEY_DIFFUSE_STR "$clr.diffuse"
#define AI_MATKEY_EMISSIVE_STR "$clr.emissive"
#define AI_MATKEY_AMBIENT_STR "$clr.ambient"

Model::Node::Node(Transform* tx, Node* parent, const aiNode& self, const Model* model) :
    model_(model),
    parent_(parent),
    name_(self.mName.C_Str())
{
    transform_ = std::make_unique<Transform>(*(Matrix4x4*)(&self.mTransformation));
    transform_->SetParent(tx);

    auto scene = model_->scene();
    meshes_.reserve(self.mNumMeshes);
    for (size_t i = 0; i < self.mNumMeshes; ++i) {
        auto mesh_index = self.mMeshes[i];
        auto mesh = scene->mMeshes[mesh_index];
        meshes_.push_back(MeshDesc{ mesh_index, mesh->mMaterialIndex });
        //bound = AABB::Union(bound, mesh->bounds());
    }
    //it works, because the world space is identical to object space when model created at origin position.
    //bounds_ = AABB::Transform(bound, transform_->LocalToWorldMatrix());

    children_.reserve(self.mNumChildren);
    for (size_t i = 0; i < self.mNumChildren; ++i) {
        auto child = self.mChildren[i];
        children_.emplace_back(transform_.get(), this, *child, model);
        //bounds_ = AABB::Union(bounds_, children_.back().bounds_);
    }
}

GameObject& Model::Node::GenerateGameObject(Transform* parent_tx, float scale)
{
    auto& go = GameObject::Create(name_.c_str());
    auto& tx = go.transform();
    tx.local_position(transform_->local_position());
    tx.local_rotation(transform_->local_rotation());
    if (!parent_tx) {
        tx.local_scale(transform_->local_scale() * scale);
    }
    else {
        tx.local_scale(transform_->local_scale());
        tx.SetParent(parent_tx);
    }

    if (meshes_.size() > 1) {
        for (auto desc : meshes_) {
            auto mesh_index = desc.mesh;
            auto mat_index = desc.material;
            auto mesh = model_->GetMesh(mesh_index);
            auto& mesh_go = GameObject::Create(mesh->name().c_str());
            auto mtl = model_->GetMaterial(mat_index);
            mesh_go.transform().SetParent(&tx);

            auto* mr = mesh_go.AddComponent<MeshRenderer>(mesh, mtl);
        }
    }
    else if (meshes_.size() == 1) {
        auto mesh_index = meshes_[0].mesh;
        auto mat_index = meshes_[0].material;
        auto mesh = model_->GetMesh(mesh_index);
        auto mtl = model_->GetMaterial(mat_index);
        auto* mr = go.AddComponent<MeshRenderer>(mesh, mtl);
    }
    
    for (auto& child : children_) {
        child.GenerateGameObject(&tx, scale);
    }
    return go;
}

Model::Model(const VertexCollection& vertices, const IndexCollection& indices, const std::shared_ptr<Material>& material) {
    auto m = std::make_shared<Mesh>(vertices, indices);
    meshes_.emplace_back(std::move(m));
    materials_.push_back(material);

    root_.meshes_.push_back({ 0, 0 });
    root_.parent_ = nullptr;
}

bool Model::GetTexture(aiMaterial* mat, aiTextureType type, unsigned int index, aiString& value, aiTextureMapMode* mode) const {
    return mat->GetTexture(type, index, &value, nullptr, nullptr, nullptr, nullptr, mode) == aiReturn_SUCCESS;
}

static WarpMode GetWrapMode(aiTextureMapMode mode) {
    switch (mode) {
    case aiTextureMapMode_Clamp:
        return WarpMode::kClamp;
    case aiTextureMapMode_Mirror:
        return WarpMode::kMirror;
    //case aiTextureMapMode_Wrap:
    default:
        return WarpMode::kRepeat;
    }
}

std::shared_ptr<Texture> Model::LoadTexture(CommandBuffer* cmd_buffer, const std::filesystem::path& base_path, aiMaterial* mtl,
    aiTextureType type, aiTextureType opt_type, const char* color_key, const Color& default_color,
    bool srgb, bool mips, TextureWarpMode& mode) const
{
    aiString name;
    aiTextureMapMode map_mode[3];

    if (GetTexture(mtl, type, 0, name, map_mode) || 
        (opt_type != aiTextureType_NONE && GetTexture(mtl, opt_type, 0, name, map_mode))) {
        auto texture_path = base_path / name.C_Str();
        mode = { GetWrapMode(map_mode[0]), GetWrapMode(map_mode[1]), GetWrapMode(map_mode[2]) };

        return cmd_buffer->CreateTextureFromFile(texture_path.c_str(), srgb, mips);
    }
    else
    {
        Color color;
        if (type == aiTextureType_UNKNOWN) {
            color = Color{ 0.0f, 0.5f, 0.0f, 1.0f };
            if (float factor; mtl->Get(AI_MATKEY_METALLIC_FACTOR, factor) == aiReturn_SUCCESS) {
                color.g = factor;
            }
            if (float factor; mtl->Get(AI_MATKEY_ROUGHNESS_FACTOR, factor) == aiReturn_SUCCESS) {
                color.r = factor;
            }
        }
        else if (color_key) {
            if (aiColor3D ai_color; mtl->Get(color_key, 0, 0, ai_color) == aiReturn_SUCCESS) {
                color = Color{ ai_color.r, ai_color.g, ai_color.b, 1.0f };
            }
            else {
                color = default_color;
            }
        }
        return cmd_buffer->CreateTextureFromColor(color, false);
    }
}

Model::Model(CommandBuffer* cmd_buffer, const char* file, bool flip_uv) {
    std::filesystem::path path(file);
    auto base_path = path.parent_path();

    uint32_t flag = aiProcess_MakeLeftHanded |
        aiProcess_FlipWindingOrder |
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_GenNormals |
        aiProcess_GenUVCoords |
        aiProcess_SortByPType |
        aiProcess_OptimizeMeshes |
        aiProcess_RemoveRedundantMaterials |
        aiProcess_CalcTangentSpace |
        aiProcess_ValidateDataStructure;

    if (flip_uv) {
        flag |= aiProcess_FlipUVs;
    }

    scene_ = importer_.ReadFile(file, flag);

    // If the import failed, report it
    if (!scene_) {
        throw std::exception(importer_.GetErrorString());
    }

    name_ = { scene_->mName.data, scene_->mName.length };

    meshes_.reserve(scene_->mNumMeshes);
    for (size_t i = 0; i < scene_->mNumMeshes; i++) {
        const auto& ai_mesh = *scene_->mMeshes[i];
        auto& mesh = meshes_.emplace_back(std::move(std::make_shared<Mesh>(ai_mesh)));
        mesh->name(ai_mesh.mName.C_Str());
    }

    materials_.reserve(scene_->mNumMaterials);
    auto renderer = App::Self()->GetRenderer();
    auto gfx = GfxDriver::Get();
    for (size_t i = 0; i < scene_->mNumMaterials; ++i) {
        PbrParam param;
        auto ai_mat = scene_->mMaterials[i];

        TextureWarpMode albedo_warp;
        auto albedo_tex = LoadTexture(cmd_buffer, base_path, ai_mat, aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE,
            AI_MATKEY_DIFFUSE_STR, Color::kWhite, true, true, albedo_warp);

        TextureWarpMode normal_warp;
        auto normal_tex = LoadTexture(cmd_buffer, base_path, ai_mat, aiTextureType_NORMALS, aiTextureType_NONE,
            nullptr, Color::kWhite, false, true, normal_warp);

        TextureWarpMode emissive_warp;
        auto emissive_tex = LoadTexture(cmd_buffer, base_path, ai_mat, aiTextureType_EMISSIVE, aiTextureType_NONE,
            AI_MATKEY_EMISSIVE_STR, Color::kBlack, true, true, emissive_warp);

        TextureWarpMode ao_warp;
        auto ao_tex = LoadTexture(cmd_buffer, base_path, ai_mat, aiTextureType_AMBIENT_OCCLUSION, aiTextureType_LIGHTMAP,
            AI_MATKEY_AMBIENT_STR, Color::kWhite, false, true, ao_warp);

        TextureWarpMode metal_roughness_warp;
        auto metal_roughness_tex = LoadTexture(cmd_buffer, base_path, ai_mat, aiTextureType_UNKNOWN, aiTextureType_NONE,
            nullptr, Color::kWhite, false, true, metal_roughness_warp);

        auto mat = renderer->CreateLightingMaterial(ai_mat->GetName().C_Str());
        LOG_LOG("material {}:", ai_mat->GetName().C_Str());

        mat->SetProperty("AlbedoTexture", albedo_tex);
        mat->SetProperty("EmissiveTexture", emissive_tex);
        mat->SetProperty("AoTexture", ao_tex);
        mat->SetProperty("MetalRoughnessTexture", metal_roughness_tex);
        mat->SetProperty("NormalTexture", normal_tex);

        if (normal_tex->IsFileImage()) {
            param.use_normal_map = 1;
        }

        auto mat_cbuf = gfx->CreateConstantParameter<PbrParam, UsageType::kDefault>(param);
        mat->SetProperty("object_material", mat_cbuf);

        if (albedo_warp != emissive_warp || 
            emissive_warp != ao_warp || 
            ao_warp != metal_roughness_warp ||
            (normal_tex->IsFileImage() && ao_warp != normal_warp))
        {
            LOG_WARN("Texture Warp Mode inconsist for {}", mat->name());
        }

        SamplerState ss;
        ss.warpU = albedo_warp[0];
        ss.warpV = albedo_warp[1];
        ss.warpW = albedo_warp[2];
        mat->SetProperty("linear_sampler", ss);

        materials_.push_back(mat);
    }

    root_ = Node(nullptr, nullptr, *scene_->mRootNode, this);
}

const std::shared_ptr<Mesh>& Model::GetMesh(size_t idx) const {
    if (idx < meshes_.size()) {
        return meshes_[idx];
    }

    return {};
}

const std::shared_ptr<Material>& Model::GetMaterial(size_t idx) const {
    if (idx < materials_.size()) {
        return materials_[idx];
    }

    return {};
}

GameObject& Model::GenerateGameObject(float scale) {
    return root_.GenerateGameObject(nullptr, scale);
}

GameObject& Model::GenerateGameObject(CommandBuffer* cmd_buffer, const char* file, bool flip_uv, float scale) {
    Model model(cmd_buffer, file, flip_uv);
    return model.GenerateGameObject(scale);
}

}
}
