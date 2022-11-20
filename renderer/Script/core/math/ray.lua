local setmetatable = setmetatable
local tostring = tostring
local Vector3 = require("core.math.vector3")
local sformat = string.format
local new_vector3 = Vector3.new

local Ray = {}

-- luacheck: ignore self
local function new_ray(origin, direction)
	local o = {origin = origin, direction = direction}
	setmetatable(o, Ray)
	return o
end

Ray.__index = Ray

Ray.__call = function(origin, direction)
    return new_ray(origin, direction)
end

Ray.__tostring = function(self)
	return sformat("Ray [%s, %s]", tostring(self.origin), tostring(self.direction))
end

function Ray.new_ex(x1, y1, z1, x2, y2, z2)
    return new_ray(new_vector3(x1, y1, z1), new_vector3(x2, y2, z2))
end

function Ray.new(origin, direction)
    return new_ray(origin, direction)
end

function Ray:point(d)
    return self.origin + self.direction * d
end

return Ray

