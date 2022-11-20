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

local pbr_scene = Scene("pbr")

pbr_scene:OnLoad(function(renderer)
    local camera_go = GameObject.Create("MainCamera"):gc_disable()
    local camera = Camera(CameraType.kPersp):gc_disable()
    pbr_scene:SetMainCamera(camera)

    camera_go:AddComponentPtr(camera)
    local cam_ctrl = CameraController(camera):gc_disable()
    camera_go:AddComponentPtr(cam_ctrl)

    camera.position = { 102.0, 25.5, 4.5 }
    camera.rotation = Quaternion.euler(9.4, 266.671, 0)

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
    helmet:GetTransform().position = { 99, 25, 4 }

    local pbr_floor = MaterialManager.Instance():Get("pbr_floor");

    local ground_go = Primitive.CreateCube(pbr_floor, {200.0, 1.0, 200.0}):gc_disable()
    ground_go.name = "Ground"
    ground_go:GetTransform().position = { 0.0, -1.5, 0.0 }

    cmd_queue:LExecuteCommandBuffer(cmd_buffer)
    cmd_queue:LFlush()
end)

return pbr_scene
