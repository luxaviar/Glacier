local Scene = require("Glacier.Scene")
local GameObject = require("Glacier.GameObject")
local Camera = require("Glacier.Camera")
local CameraType = require("Glacier.CameraType")
local Quaternion = require("core.math.quaternion")
local Vector3 = require("core.math.vector3")
local CameraController = require("Glacier.CameraController")
local DirectionalLight = require("Glacier.DirectionalLight")
local CommandBufferType = require("Glacier.CommandBufferType")
local Model = require("Glacier.Model")
local MaterialManager = require("Glacier.MaterialManager")
local Primitive = require("Glacier.Primitive")
local Fall = require("Glacier.Fall")
local BoxCollider = require("Glacier.BoxCollider")

local phy_scene = Scene("physics")

phy_scene:OnLoad(function(renderer)
    local camera_go = GameObject.Create("MainCamera"):gc_disable()
    local camera = Camera(CameraType.kPersp):gc_disable()
    phy_scene:SetMainCamera(camera)

    camera_go:AddComponentPtr(camera)
    local cam_ctrl = CameraController(camera):gc_disable()
    camera_go:AddComponentPtr(cam_ctrl)

    -- press F to drop colliders
    local fall = Fall():gc_disable()
    camera_go:AddComponentPtr(fall)

    camera.position = { 11.0, 8.0, -20.0 }
    camera:LookAt({ 0.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 })
    camera.fov = 45

    local light_go = GameObject.Create("main_light"):gc_disable()
    local dir = (Vector3(1, -1, 0) - Vector3.zero):normalize()
    light_go:GetTransform().forward = dir;

    local main_light = DirectionalLight({1, 1, 1, 1}, 1.0):gc_disable()
    main_light:EnableShadow()
    light_go:AddComponentPtr(main_light);

    local gfx = renderer:driver()
    local cmd_queue = gfx:LGetCommandQueue(CommandBufferType.kDirect)
    local cmd_buffer = cmd_queue:GetCommandBuffer()

    local helmet = Model.GenerateGameObject(cmd_buffer, "assets\\model\\helmet\\DamagedHelmet.gltf", true, 1.0):gc_disable()
    helmet:GetTransform().position = { 7.5, 6.0, -14.0 }

    local pbr_floor = MaterialManager.Instance():Get("pbr_floor");

    local ground_go = Primitive.CreateCube(pbr_floor, {50.0, 2.0, 50.0}):gc_disable()
    ground_go.name = "floor"
    ground_go:GetTransform().position = { 0.0, -1.5, 0.0 }

    local collider = BoxCollider({ 25.0, 1.0, 25.0 }):gc_disable()
    ground_go:AddComponentPtr(collider)

    cmd_queue:LExecuteCommandBuffer(cmd_buffer)
    cmd_queue:LFlush()
end)

return phy_scene
