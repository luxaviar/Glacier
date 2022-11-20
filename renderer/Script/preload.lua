package.path = package.path .. ";Script/?.lua"

require("core.extension.assert")
require("core.extension.error")
require("core.utils")

local Vector2 = require("core.math.vector2")
local Vector3 = require("core.math.vector3")
local Color = require("core.math.color")
local Quaternion = require("core.math.quaternion")
local Ray = require("core.math.ray")
local AABB = require("core.math.aabb")
local RayHitResult = require("core.math.hitresult")

local LightweightValue = require("Glacier.LightweightValue")
local Checker = require("Glacier.Checker")
Checker.Register(LightweightValue.kVector2, Vector2.new)
Checker.Register(LightweightValue.kVector3, Vector3.new)
Checker.Register(LightweightValue.kQuaternion, Quaternion.new)
Checker.Register(LightweightValue.kColor, Color.new)
Checker.Register(LightweightValue.kRay, Ray.new_ex)
Checker.Register(LightweightValue.kAABB, AABB.new_ex)
Checker.Register(LightweightValue.kRayHitResult, RayHitResult.new)