local setmetatable = setmetatable

local Color = {}

local function new_color(r, g, b, a)
	local o = {r or 0, g or 0, b or 0, a or 1}
	setmetatable(o, Color)
	return o
end

setmetatable(Color, {
    __call=function(_, r, g, b, a) return new_color(r, g, b, a) end
})

Color.__index = Color

Color.__call = function(_, r, g, b, a)
    return new_color(r, g, b, a)
end

Color.__mul = function(lhs, rhs)
    return new_color(lhs[1] * rhs, lhs[2] * rhs, lhs[3] * rhs, lhs[4] * rhs)
end

Color.__unm = function(q)
	return new_color(-q[1], -q[2], -q[3], -q[4])
end

Color.__tostring = function(self)
	return "["..self[1]..","..self[2]..","..self[3]..","..self[4].."]"
end

function Color.new(x, y, z, w)
    return new_color(x, y, z, w)
end

function Color:r()
    return Color[1]
end

function Color:g()
    return Color[2]
end

function Color:b()
    return Color[3]
end

function Color:a()
    return Color[4]
end

function Color:set(r,g,b,a)
	Color[1] = r or 0
	Color[2] = g or 0
	Color[3] = b or 0
	Color[4] = a or 0
end

function Color:clone()
	return new_color(Color[1], Color[2], Color[3], Color[4])
end

function Color.equals(a, b)
	return a[1] == b[1] and a[2] == b[2] and a[3] == b[3] and a[4] == b[4]
end

Color.identity = new_color(0, 0, 0, 1)
Color.euler_angles = Color.to_euler_angles

return Color

