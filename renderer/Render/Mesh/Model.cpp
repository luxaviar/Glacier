#include "Model.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include "Render/Graph/PassNode.h"
#include "Common/Util.h"
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags
#include "Core/GameObject.h"
#include "Common/Log.h"
#include <assimp/GltfMaterial.h>
#include "App.h"
#include "Render/Renderer.h"
#include "Animation/Animator.h"
#include "Animation/AnimationClipCache.h"
#include "Render/Mesh/SkinnedMeshRenderer.h"
#include "Lux/Lux.h"

namespace glacier {
namespace render {

LUX_IMPL(Model, Model)
LUX_CTOR(Model, CommandBuffer*, const char*, bool)
LUX_FUNC(Model, GenerateGameObject)
LUX_IMPL_END

#define AI_MATKEY_DIFFUSE_STR "$clr.diffuse"
#define AI_MATKEY_EMISSIVE_STR "$clr.emissive"
#define AI_MATKEY_AMBIENT_STR "$clr.ambient"

namespace {

//Models are imported once per file and shared by all of their instances; the
//entry is dropped when the source file changed.
struct CachedModel {
    std::shared_ptr<Model> model;
    FileStamp stamp;
};

std::unordered_map<std::string, CachedModel> g_model_cache;

std::string ModelCacheKey(const char* file, bool flip_uv) {
    std::error_code error;
    auto path = std::filesystem::weakly_canonical(file, error);

    std::string key = error ? file : path.string();
    return flip_uv ? key + "|flip_uv" : key;
}

//taken when the importer does not provide a tick rate (assimp documents 0 as "not time based")
constexpr double kDefaultTicksPerSecond = 25.0;

AnimationInterpolation ToInterpolation(aiAnimInterpolation v) {
    //assimp's key structures carry no tangents, so cubic spline keys degrade to linear
    return v == aiAnimInterpolation_Step ?
        AnimationInterpolation::kStep : AnimationInterpolation::kLinear;
}

float ToSeconds(double time, double ticks_per_second) {
    double ticks = ticks_per_second > 0.0 ? ticks_per_second : kDefaultTicksPerSecond;
    return (float)(time / ticks);
}

void AddMeshComponent(GameObject& go, const std::shared_ptr<Mesh>& mesh, const std::shared_ptr<Material>& material,
    const std::shared_ptr<NodeTransformTable>& nodes) {
    if (mesh->IsSkinned()) {
        //the bones of the mesh address the node table by index
        auto* renderer = go.AddComponent<SkinnedMeshRenderer>(mesh, material);
        renderer->SetNodeTable(nodes);
    }
    else {
        go.AddComponent<MeshRenderer>(mesh, material);
    }
}

//aiProcess_MakeLeftHanded converts node transforms and animation values together,
//so the imported keys can be used as-is.
std::shared_ptr<AnimationClip> ImportAnimation(const aiAnimation& anim, size_t index) {
    std::string clip_name = anim.mName.C_Str();
    if (clip_name.empty()) {
        clip_name = "Animation" + std::to_string(index);
    }

    auto clip = std::make_shared<AnimationClip>(clip_name.c_str());
    auto ticks_per_second = anim.mTicksPerSecond;

    for (size_t i = 0; i < anim.mNumChannels; ++i) {
        const auto& channel = *anim.mChannels[i];
        auto& track = clip->AddTrack(channel.mNodeName.C_Str());

        for (size_t k = 0; k < channel.mNumPositionKeys; ++k) {
            const auto& key = channel.mPositionKeys[k];
            track.AddPosition(Vec3Keyframe{
                ToSeconds(key.mTime, ticks_per_second),
                Vec3f{ (float)key.mValue.x, (float)key.mValue.y, (float)key.mValue.z },
                ToInterpolation(key.mInterpolation) });
        }

        for (size_t k = 0; k < channel.mNumRotationKeys; ++k) {
            const auto& key = channel.mRotationKeys[k];
            track.AddRotation(QuatKeyframe{
                ToSeconds(key.mTime, ticks_per_second),
                Quaternion{ (float)key.mValue.x, (float)key.mValue.y, (float)key.mValue.z, (float)key.mValue.w },
                ToInterpolation(key.mInterpolation) });
        }

        for (size_t k = 0; k < channel.mNumScalingKeys; ++k) {
            const auto& key = channel.mScalingKeys[k];
            track.AddScale(Vec3Keyframe{
                ToSeconds(key.mTime, ticks_per_second),
                Vec3f{ (float)key.mValue.x, (float)key.mValue.y, (float)key.mValue.z },
                ToInterpolation(key.mInterpolation) });
        }
    }

    return clip;
}

}

void Model::CollectNodes(const Node& node, int32_t parent, const Matrix4x4& parent_world, std::vector<NodeInfo>& out) {
    NodeInfo info;
    info.node = &node;
    info.parent = parent;
    info.world_bind = parent_world * node.transform_->LocalToParentMatrix();

    out.push_back(info);
    int32_t index = (int32_t)out.size() - 1;

    for (const auto& child : node.children_) {
        CollectNodes(child, index, info.world_bind, out);
    }
}

float Model::BindError(const Matrix4x4& world_bind, const Matrix4x4& offset_matrix) {
    //the bind pose of the joint has to cancel the inverse bind matrix of the skin
    const Matrix4x4 skin = world_bind * offset_matrix;

    float error = 0.0f;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            float expected = row == col ? 1.0f : 0.0f;
            error = std::max(error, std::abs(skin(row, col) - expected));
        }
    }

    return error;
}

std::vector<Mesh::Bone> Model::ResolveBones(const aiMesh& mesh, const std::string& file,
    std::vector<NodeInfo>& nodes,
    const std::unordered_map<std::string, std::vector<int32_t>>& joints) const
{
    //a joint is a node of the scene and the inverse bind matrix is the inverse
    //of that node's bind pose, which is what tells duplicated names apart
    constexpr float kJointMatchEpsilon = 1.0e-3f;

    std::vector<Mesh::Bone> bones;
    bones.reserve(mesh.mNumBones);

    size_t missing = 0;
    size_t matched_by_bind = 0;
    std::string first_missing;

    for (size_t b = 0; b < mesh.mNumBones; ++b) {
        const auto& ai_bone = *mesh.mBones[b];

        Mesh::Bone bone;
        std::memcpy(&bone.offset_matrix, &ai_bone.mOffsetMatrix, sizeof(bone.offset_matrix));

        auto it = joints.find(ai_bone.mName.C_Str());
        if (it == joints.end()) {
            if (missing == 0) {
                first_missing = ai_bone.mName.C_Str();
            }
            ++missing;
            bones.push_back(bone);
            continue;
        }

        //a name can belong to more than one node, or to a node that is not the
        //joint at all, so the bind pose of the joint picks between the candidates
        int32_t matched = it->second.front();
        float error = BindError(nodes[matched].world_bind, bone.offset_matrix);

        for (size_t c = 1; c < it->second.size(); ++c) {
            float candidate_error = BindError(nodes[it->second[c]].world_bind, bone.offset_matrix);
            if (candidate_error < error) {
                error = candidate_error;
                matched = it->second[c];
            }
        }

        bone.node = matched;

        if (error >= kJointMatchEpsilon) {
            if (it->second.size() > 1) {
                LOG_ERR("model '{0}': joint '{1}' is one of {2} nodes with that name but none of them matches its bind pose",
                    file, ai_bone.mName.C_Str(), it->second.size());
            }
            else {
                LOG_WARN("model '{0}': joint '{1}' is bound to a node whose bind pose does not match the skin (error {2:.3f})",
                    file, ai_bone.mName.C_Str(), error);
            }
        }
        else if (it->second.size() > 1) {
            ++matched_by_bind;
        }

        //the skeleton carries the inverse bind matrix the skin gave the joint
        auto& node = nodes[(size_t)bone.node];
        if (!node.has_inverse_bind) {
            node.has_inverse_bind = true;
            node.inverse_bind = bone.offset_matrix;
        }

        bones.push_back(bone);
    }

    if (missing > 0) {
        LOG_ERR("model '{0}': mesh '{1}' has {2} joint(s) that are not nodes of the scene, first is '{3}'; they keep the bind pose",
            file, mesh.mName.C_Str(), missing, first_missing);
    }

    if (matched_by_bind > 0) {
        LOG_LOG("model '{0}': mesh '{1}' matched {2} joint(s) with a shared name by their bind pose",
            file, mesh.mName.C_Str(), matched_by_bind);
    }

    return bones;
}

std::shared_ptr<Skeleton> Model::BuildSkeleton(const std::vector<NodeInfo>& nodes) const {
    //every node of the imported tree becomes a bone, so clips can address nodes
    //that are not joints by name as well
    auto skeleton = std::make_shared<Skeleton>();

    for (const auto& info : nodes) {
        const auto& tx = info.node->transform();
        skeleton->AddBone(info.node->name_.c_str(), info.parent,
            tx.local_position(), tx.local_rotation(), tx.local_scale(), info.inverse_bind);
    }

    skeleton->Build();
    return skeleton;
}

Model::Node::Node(Transform* tx, Node* parent, const aiNode& self, const Model* model) :
    model_(model),
    parent_(parent),
    //nodes are numbered depth first, the same order the skeleton keeps its bones
    index_(model->node_count_++),
    name_(self.mName.C_Str())
{
    transform_ = std::make_unique<Transform>(*(Matrix4x4*)(&self.mTransformation));
    transform_->SetParent(tx);

    auto scene = model_->scene_;
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

GameObject& Model::Node::GenerateGameObject(Transform* parent_tx, float scale, const std::shared_ptr<NodeTransformTable>& nodes)
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

    if (nodes) {
        (*nodes)[index_] = &tx;
    }

    if (meshes_.size() > 1) {
        for (auto desc : meshes_) {
            auto mesh_index = desc.mesh;
            auto mat_index = desc.material;
            auto mesh = model_->GetMesh(mesh_index);
            auto& mesh_go = GameObject::Create(mesh->name().c_str());
            auto mtl = model_->GetMaterial(mat_index);
            mesh_go.transform().SetParent(&tx);

            AddMeshComponent(mesh_go, mesh, mtl, nodes);
        }
    }
    else if (meshes_.size() == 1) {
        auto mesh_index = meshes_[0].mesh;
        auto mat_index = meshes_[0].material;
        auto mesh = model_->GetMesh(mesh_index);
        auto mtl = model_->GetMaterial(mat_index);
        AddMeshComponent(go, mesh, mtl, nodes);
    }
    
    for (auto& child : children_) {
        child.GenerateGameObject(&tx, scale, nodes);
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
        Color color = default_color;
        if (type == aiTextureType_UNKNOWN) {
            color = Color{ 0.0f, 0.5f, 0.0f, 1.0f };
            if (float factor; mtl->Get(AI_MATKEY_METALLIC_FACTOR, factor) == aiReturn_SUCCESS) {
                color.b = factor;
            }
            if (float factor; mtl->Get(AI_MATKEY_ROUGHNESS_FACTOR, factor) == aiReturn_SUCCESS) {
                color.g = factor;
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
        aiProcess_LimitBoneWeights |
        aiProcess_SortByPType |
        aiProcess_OptimizeMeshes |
        aiProcess_RemoveRedundantMaterials |
        aiProcess_CalcTangentSpace |
        aiProcess_ValidateDataStructure;

    if (flip_uv) {
        flag |= aiProcess_FlipUVs;
    }

    importer_ = std::make_unique<Assimp::Importer>();
    scene_ = importer_->ReadFile(file, flag);

    // If the import failed, report it
    if (!scene_) {
        throw std::exception(importer_->GetErrorString());
    }

    name_ = { scene_->mName.data, scene_->mName.length };

    //the node tree comes first: it numbers the nodes, and those numbers are the
    //bone indices the meshes, the skeleton and the animator share
    root_ = Node(nullptr, nullptr, *scene_->mRootNode, this);

    std::vector<NodeInfo> nodes;
    nodes.reserve(node_count_);
    CollectNodes(root_, kInvalidBoneIndex, Matrix4x4::identity, nodes);

    //joint names of the scene; a name used by more than one node keeps every
    //candidate, so the bind pose of the joint can pick between them
    std::unordered_map<std::string, std::vector<int32_t>> joints;
    size_t shared_names = 0;
    for (size_t i = 0; i < nodes.size(); ++i) {
        auto& candidates = joints[nodes[i].node->name_];
        if (!candidates.empty()) {
            ++shared_names;
        }
        candidates.push_back((int32_t)i);
    }

    if (shared_names > 0) {
        LOG_WARN("model '{0}': {1} node name(s) belong to more than one node; bones bind by index, clips that address them by name always hit the first one",
            path.filename().string(), shared_names);
    }

    bool skinned = false;
    for (size_t i = 0; i < scene_->mNumMeshes; ++i) {
        skinned = skinned || scene_->mMeshes[i]->mNumBones > 0;
    }

    meshes_.reserve(scene_->mNumMeshes);
    for (size_t i = 0; i < scene_->mNumMeshes; i++) {
        const auto& ai_mesh = *scene_->mMeshes[i];
        auto& mesh = meshes_.emplace_back(std::make_shared<Mesh>(ai_mesh,
            ResolveBones(ai_mesh, path.filename().string(), nodes, joints)));
        mesh->name(ai_mesh.mName.C_Str());
    }

    if (skinned || scene_->mNumAnimations > 0) {
        skeleton_ = BuildSkeleton(nodes);
        LOG_LOG("skeleton: {} bones", skeleton_->bone_count());
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
        auto metal_roughness_tex = LoadTexture(cmd_buffer, base_path, ai_mat, aiTextureType_GLTF_METALLIC_ROUGHNESS, aiTextureType_NONE,
            nullptr, Color{ 0.0f, 1.0f, 0.0f, 1.0f }, false, true, metal_roughness_warp);

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

    ImportAnimations(path);

    //the parsed scene is only needed while the model is built; the model itself
    //stays in the import cache, and keeping the source file in memory with it
    //would cost more than everything the model uses
    importer_.reset();
    scene_ = nullptr;
}

void Model::ImportAnimations(const std::filesystem::path& path) {
    if (scene_->mNumAnimations == 0) {
        return;
    }

    if (!AnimationClipCache::Load(path, animations_)) {
        animations_.reserve(scene_->mNumAnimations);
        for (size_t i = 0; i < scene_->mNumAnimations; ++i) {
            animations_.emplace_back(ImportAnimation(*scene_->mAnimations[i], i));
            LOG_LOG("animation {0}: {1} tracks, {2}s",
                animations_.back()->name(), animations_.back()->track_count(), animations_.back()->duration());
        }

        AnimationClipCache::Save(path, animations_);
    }

    //sampling addresses the bones of this model by index, so every track resolves
    //its node once here; a clip played on another skeleton falls back to names
    for (auto& clip : animations_) {
        if (clip && skeleton_) {
            clip->BindToSkeleton(*skeleton_);
        }
    }
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

GameObject& Model::CreateGameObject(float scale) {
    //every instance owns its table of node transforms: the bones of its skinned
    //meshes and its animator address that table by index, so two instances (or
    //two nodes that share a name) cannot bind to each other
    std::shared_ptr<NodeTransformTable> nodes;
    if (skeleton_) {
        ASSERT(node_count_ == skeleton_->bone_count());
        nodes = std::make_shared<NodeTransformTable>(node_count_, nullptr);
    }

    auto& go = root_.GenerateGameObject(nullptr, scale, nodes);

    if (!animations_.empty()) {
        auto* animator = go.AddComponent<Animator>();
        animator->SetClips(animations_);
        animator->SetSkeleton(skeleton_);
        animator->BindNodes(go.transform(), nodes);
    }

    return go;
}

GameObject& Model::GenerateGameObject(CommandBuffer* cmd_buffer, const char* file, bool flip_uv, float scale) {
    return Load(cmd_buffer, file, flip_uv)->CreateGameObject(scale);
}

std::shared_ptr<Model> Model::Load(CommandBuffer* cmd_buffer, const char* file, bool flip_uv) {
    const std::string key = ModelCacheKey(file, flip_uv);
    const FileStamp stamp = StampOfFile(file);

    auto it = g_model_cache.find(key);
    if (it != g_model_cache.end() && it->second.stamp == stamp) {
        LOG_DEBUG("model '{}': reusing the imported asset", file);
        return it->second.model;
    }

    auto model = std::make_shared<Model>(cmd_buffer, file, flip_uv);

    CachedModel entry;
    entry.model = model;
    entry.stamp = stamp;
    g_model_cache[key] = std::move(entry);

    return model;
}

void Model::ClearCache() {
    g_model_cache.clear();
}

}
}
