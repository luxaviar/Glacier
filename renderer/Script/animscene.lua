local Scene = require("Glacier.Scene")
local GameObject = require("Glacier.GameObject")
local Camera = require("Glacier.Camera")
local CameraType = require("Glacier.CameraType")
local Vector3 = require("core.math.vector3")
local CameraController = require("Glacier.CameraController")
local DirectionalLight = require("Glacier.DirectionalLight")
local CommandBufferType = require("Glacier.CommandBufferType")
local Model = require("Glacier.Model")
local MaterialManager = require("Glacier.MaterialManager")
local Primitive = require("Glacier.Primitive")

-- animation demo:
--   DemoAnim.gltf  - a clip plus an additive layer on one node skeleton
--   DemoSkin.gltf  - crossfade from one clip into another, the ribbon bends
--                    through poses neither clip reaches on its own
local anim_scene = Scene("anim")

anim_scene:OnLoad(function(renderer)
    local camera_go = GameObject.Create("MainCamera"):gc_disable()
    local camera = Camera(CameraType.kPersp):gc_disable()
    anim_scene:SetMainCamera(camera)

    camera_go:AddComponentPtr(camera)
    local cam_ctrl = CameraController(camera):gc_disable()
    camera_go:AddComponentPtr(cam_ctrl)

    camera.position = { 0.0, 3.2, -6.5 }
    camera:LookAt({ 0.0, 0.6, 0.8 }, { 0.0, 1.0, 0.0 })
    camera.fov = 45

    local light_go = GameObject.Create("main_light"):gc_disable()
    local dir = (Vector3(1, -1, 1) - Vector3.zero):normalize()
    light_go:GetTransform().forward = dir;

    local main_light = DirectionalLight({1, 1, 1, 1}, 1.0):gc_disable()
    main_light:EnableShadow()
    light_go:AddComponentPtr(main_light);

    local gfx = renderer:driver()
    local cmd_queue = gfx:LGetCommandQueue(CommandBufferType.kDirect)
    local cmd_buffer = cmd_queue:GetCommandBuffer()

    -- node transform animation: both clips are sampled every frame, the tilt is
    -- layered on top of the spin as a delta from the rest pose
    local animated_go = Model.GenerateGameObject(cmd_buffer, "assets\\model\\anim\\DemoAnim.gltf", false, 1.0):gc_disable()
    animated_go:GetTransform().position = { -1.2, 0.0, 0.0 }

    local animator = animated_go:GetAnimator()
    if animator then
        INFO("[Lua] '%s': %d clips, %d bones", animated_go.name, animator:clip_count(), animator:bone_count())
        animator:SetLoop(true)
        animator:SetSpeed(1.0)
        animator:PlayClip("DemoSpin")
        animator:SetWeight("DemoTilt", 1.0, true)

        for i = 0, animator:active_clip_count() - 1 do
            INFO("[Lua]   mixing '%s' at %.2f", animator:active_clip_name(i), animator:active_clip_weight(i))
        end
    end

    -- skinned mesh: the same clip system drives the joint transforms, and the
    -- crossfade blends the sampled poses instead of snapping between them
    local skinned_go = Model.GenerateGameObject(cmd_buffer, "assets\\model\\anim\\DemoSkin.gltf", false, 1.0):gc_disable()
    skinned_go:GetTransform().position = { 1.2, 0.0, 0.0 }

    local skinned_animator = skinned_go:GetAnimator()
    if skinned_animator then
        INFO("[Lua] '%s': %d clips, %d bones", skinned_go.name, skinned_animator:clip_count(), skinned_animator:bone_count())
        skinned_animator:SetLoop(true)
        skinned_animator:PlayClip("SkinWave")
        skinned_animator:CrossFade("SkinCoil", 4.0)
        INFO("[Lua] crossfading '%s' -> '%s' over 4.00s",
            skinned_animator:active_clip_name(0), skinned_animator:active_clip_name(1))
    end

    -- instanced skinned meshes: the copies share the mesh and the material of
    -- the model, so the renderer draws them with one instanced draw call; every
    -- instance has its own animator, its own pose and its own slot in the shared
    -- bone matrix pool. A batch draws the bind pose of the mesh, so the compute
    -- skinning of those instances is off and the vertex shader of the pass skins
    -- them with the bones of their instance.
    for i = 1, 3 do
        local instance_go = Model.GenerateGameObject(cmd_buffer, "assets\\model\\anim\\DemoSkin.gltf", false, 1.0):gc_disable()
        instance_go:GetTransform().position = { 1.2, 0.0, -2.0 * i }

        local skinned_renderer = instance_go:GetSkinnedMeshRenderer()
        if skinned_renderer then
            -- turning instancing on turns the compute skinning of this object
            -- off, because a batch has to draw the bind pose of the mesh
            skinned_renderer:SetInstancing(true)
        end

        local instance_animator = instance_go:GetAnimator()
        if instance_animator then
            instance_animator:SetLoop(true)
            instance_animator:PlayClip("SkinWave")
            -- a different phase and speed per instance, so one draw call
            -- carries three different poses
            instance_animator:SetTime(0.9 * i)
            instance_animator:SetSpeed(0.6 + 0.2 * i)
        end
    end

    local pbr_floor = MaterialManager.Instance():Get("pbr_floor");

    local ground_go = Primitive.CreateCube(pbr_floor, { 40.0, 1.0, 40.0 }):gc_disable()
    ground_go.name = "Ground"
    ground_go:GetTransform().position = { 0.0, -1.5, 0.0 }

    cmd_queue:LExecuteCommandBuffer(cmd_buffer)
    cmd_queue:LFlush()
end)

return anim_scene
