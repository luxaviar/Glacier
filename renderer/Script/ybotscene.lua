local Scene = require("Glacier.Scene")
local GameObject = require("Glacier.GameObject")
local Camera = require("Glacier.Camera")
local CameraType = require("Glacier.CameraType")
local Vector3 = require("core.math.vector3")
local ThirdPersonCamera = require("Glacier.ThirdPersonCamera")
local DirectionalLight = require("Glacier.DirectionalLight")
local CommandBufferType = require("Glacier.CommandBufferType")
local Model = require("Glacier.Model")
local MaterialManager = require("Glacier.MaterialManager")
local Primitive = require("Glacier.Primitive")
local YBotDemo = require("Glacier.YBotDemo")
local PlayerController = require("Glacier.PlayerController")
local DemoHud = require("Glacier.DemoHud")

-- playable animation demo on a real humanoid rig: one Mixamo Y Bot and its
-- locomotion pack, merged into a single glTF by tools/mixamo_to_gltf.py,
-- because a model and the clips an animator can play have to come from the
-- same file.
--
-- third person controls of an action game: W walks away from the view, S walks
-- towards it, A and D walk across it, shift switches to running, and holding
-- the right mouse button swings the camera around the character without
-- turning it; the wheel pulls the camera in and out. The character turns
-- towards the way it walks, and PlayerController mixes the clips of the pack -
-- idle, walking, the two strafes and running - from the movement measured in
-- the frame of the character. Space jumps: the clip of the pack crouches,
-- the controller carries the character through the air under its own gravity,
-- and the landing of the clip is blended back into the mix. P pauses the app
-- (see App::HandleInput), space belongs to the character.
--
-- the same file can also be driven by YBotDemo, which plays the features of the
-- animator (1D blend tree, crossfade, root motion) as a scripted timeline
-- instead of following the keyboard; see the bottom of OnLoad
--
-- one instance takes 35434 of the 599186 vertices of the gpu skinning pool
-- (GpuSkinning::kCapacity, which is 32MB of skinned vertices), so the pool has
-- room for 16 Y Bots on the compute skinning path; more of them would fall back
-- to the vertex shader skinning. the bone matrix pool has room for 250 of them,
-- so the vertices run out first
local ybot_scene = Scene("ybot")

ybot_scene:OnLoad(function(renderer)
    local camera_go = GameObject.Create("MainCamera"):gc_disable()
    local camera = Camera(CameraType.kPersp):gc_disable()
    ybot_scene:SetMainCamera(camera)

    camera_go:AddComponentPtr(camera)

    -- the rig faces +Z (the exporter turns it so that the importer, which
    -- reflects the scene in Z, lands it on the engine's forward), so the camera
    -- starts behind it; ThirdPersonCamera picks the orbit angles up from
    -- wherever the scene aims the camera here, and the controller opens with
    -- the character facing the way the camera looks (its back to the view)
    camera.position = { 0.0, 1.6, -6.0 }
    camera:LookAt({ 0.0, 0.85, 0.0 }, { 0.0, 1.0, 0.0 })
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

    -- the model, its clips and its skeleton are cached and shared from the
    -- cache; the instance owns its animator, its pose and its slot of the bone
    -- matrix pool
    local ybot_go = Model.GenerateGameObject(cmd_buffer, "assets\\model\\ybot\\YBot.gltf", false, 1.0):gc_disable()
    ybot_go.name = "YBot"
    ybot_go:GetTransform().position = { 0.0, 0.0, 0.0 }

    -- third person camera: the right mouse button swings the view around the
    -- character and the wheel moves it in and out. The controller below reads
    -- WASD from it, so this has to be enabled first
    local camera_follow = ThirdPersonCamera(camera):gc_disable()
    camera_follow:SetTarget(ybot_go)
    camera_follow.distance = 4.5
    camera_follow.height = 1.15
    -- how tightly the camera follows the character; 0 snaps it where it belongs
    camera_follow.follow_speed = 10.0
    camera_go:AddComponentPtr(camera_follow)

    local player = PlayerController():gc_disable()
    player:SetCamera(camera_follow)
    -- how quickly the character swings around to the way it walks, and how fast
    -- it picks up speed and sheds it again, in m/s^2: the walk to run of shift
    -- is the ramp of the speed, the pose follows it
    player.turn_speed = 14.0
    player.accel = 12.0
    player.decel = 16.0
    -- how far the character has to be turned before it pivots where it stands
    -- with the turn clips of the pack (a quarter turn of 0.9s and a turn of
    -- about 176 degrees of 1.6s) instead of walking the turn, in degrees
    player.turn_angle = 65.0
    ybot_go:AddComponentPtr(player)

    -- the keys and the state of the demo, over the scene: H hides it, and it
    -- takes no input of its own, so the character keeps the keyboard while it is
    -- up (an ImGui window would take it as soon as it is the one in focus)
    local hud = DemoHud():gc_disable()
    hud:SetPlayer(player)
    hud:SetCamera(camera_follow)
    camera_go:AddComponentPtr(hud)

    -- the scripted timeline of the animator features, to watch instead of drive:
    -- uncomment it and drop the controller above
    --local demo = YBotDemo():gc_disable()
    --demo:UseTimeline()
    --demo:SetBlendRange(0.0, 2.0, 7.0)
    --ybot_go:AddComponentPtr(demo)

    local pbr_floor = MaterialManager.Instance():Get("pbr_floor");

    local ground_go = Primitive.CreateCube(pbr_floor, { 40.0, 1.0, 40.0 }):gc_disable()
    ground_go.name = "Ground"
    ground_go:GetTransform().position = { 0.0, -0.5, 0.0 }

    cmd_queue:LExecuteCommandBuffer(cmd_buffer)
    cmd_queue:LFlush()
end)

return ybot_scene
