local main = {}
local INFO = INFO

function main.init(cmd_args)
    local App = require("Glacier.App")
    local app = App:Self()

    local Window = require("Glacier.Window")
    local wnd = Window(1280, 720, "Engine Viewer")
    INFO("[Lua] Create Window!")

    local TextureFormat = require("Glacier.TextureFormat")
    local D3D12GfxDriver = require("Glacier.D3D12GfxDriver")
    local gfx = D3D12GfxDriver.Instance()
    gfx:Init(wnd:handle(), wnd:width(), wnd:height(), TextureFormat.kR8G8B8A8_UNORM)
    INFO("[Lua] Create D3D12GfxDriver!")

    local DeferredRenderer = require("Glacier.DeferredRenderer")
    local AntiAliasingType = require("Glacier.AntiAliasingType")
    local renderer = DeferredRenderer(gfx, AntiAliasingType.kTAA)
    INFO("[Lua] Create DeferredRenderer!")

    app:Setup(wnd, gfx, renderer)

    INFO("[Lua] app setup completed!")

    main.create_scene()
end

function main.create_scene()
    local SceneManager = require("Glacier.SceneManager")
    local SceneLoadMode = require("Glacier.SceneLoadMode")
    local pbr_scene = require("pbrscene")
    local phy_scene = require("physcene")

    local sm = SceneManager.Instance()
    sm:Add(pbr_scene)
    sm:Add(phy_scene)
    sm:Load("pbr", SceneLoadMode.kSingle)
    --sm:Load("physics", SceneLoadMode.kSingle)
end

return main