local math  = math
local acos	= math.acos
local sqrt 	= math.sqrt
local max 	= math.max
local min 	= math.min
local clamp = math.clamp
local setmetatable = setmetatable
local getmetatable = getmetatable
local type = type
local sformat = string.format

local rad2Deg = 57.295779513082

local Vector2 = {}

local function new_vector2(x, y)
	local o = {x or 0, y or 0}
	setmetatable(o, Vector2)
	return o
end

setmetatable(Vector2, {
    __call=function(_, x, y) return new_vector2(x, y) end
})

Vector2.up 		= new_vector2(0,1)
Vector2.down 	= new_vector2(0,-1)
Vector2.right	= new_vector2(1,0)
Vector2.left	= new_vector2(-1,0)
Vector2.zero	= new_vector2(0,0)
Vector2.one		= new_vector2(1,1)

Vector2.__call = function(_, x, y)
    return new_vector2(x, y)
end

Vector2.__index = Vector2

Vector2.__tostring = function(self)
    return sformat("[%.3f, %.3f]", self[1], self[2])
end

Vector2.__div = function(va, d)
    return new_vector2(va[1] / d, va[2] / d)
end

Vector2.__mul = function(va, d)
    assert(type(d) == "number")
    return new_vector2(va[1] * d, va[2] * d)
end

Vector2.__add = function(va, vb)
    return new_vector2(va[1] + vb[1], va[2] + vb[2])
end

Vector2.__sub = function(va, vb)
    return new_vector2(va[1] - vb[1], va[2] - vb[2])
end

Vector2.__unm = function(va)
    return new_vector2(-va[1], -va[2])
end

Vector2.__eq = function(a,b)
    if getmetatable(a) ~= Vector2 or getmetatable(b) ~= Vector2 then
        return false
    end
    local v = a - b
    local delta = v:magnitude_sq()
    return delta < 1e-10
end

Vector2.new = new_vector2

function Vector2:x()
    return self[1]
end

function Vector2:y()
    return self[2]
end

function Vector2:set(x, y)
	self[1] = x or 0
	self[2] = y or 0
end

function Vector2:clone()
    return new_vector2(self[1], self[2])
end

function Vector2.distance(va, vb)
	return sqrt((va[1] - vb[1])^2 + (va[2] - vb[2])^2)
end

function Vector2.dot(lhs, rhs)
	return lhs[1] * rhs[1] + lhs[2] * rhs[2]
end

function Vector2.lerp(from, to, t)
	t = clamp(t, 0, 1)
	return new_vector2(from[1] + (to[1] - from[1]) * t, from[2] + (to[2] - from[2]) * t)
end

function Vector2:magnitude_sq()
	local x, y = self[1], self[2]
	return x * x + y * y
end

function Vector2:magnitude()
	local mag_sq = self:magnitude_sq()
	return sqrt(mag_sq)
end

function Vector2.max(lhs, rhs)
	return new_vector2(max(lhs[1], rhs[1]), max(lhs[2], rhs[2]))
end

function Vector2.min(lhs, rhs)
	return new_vector2(min(lhs[1], rhs[1]), min(lhs[2], rhs[2]))
end

function Vector2:normalized()
	local x, y = self[1], self[2]
	local num = sqrt(x * x + y * y)

	if num > 1e-5 then
        self[1] = x / num
        self[2] = y / num
    else
        self[1] = 0
        self[2] = 0
    end
end

function Vector2:normalize()
	local x, y = self[1], self[2]
	local num = sqrt(x * x + y * y)

	if num > 1e-5 then
        return new_vector2(x / num, y / num)
    else
        return new_vector2(0, 0)
    end
end

local dot = Vector2.dot

function Vector2.angle(from, to)
	return acos(clamp(dot(from:normalize(), to:normalize()), -1, 1)) * rad2Deg
end

function Vector2:limit(maxLength)
	if self:magnitude_sq() > (maxLength * maxLength) then
		self:normalized()
		self:mul(maxLength)
    end

    return self
end

function Vector2.move_towards(current, target, maxDistanceDelta)
	local delta = target - current
    local sqrDelta = delta:magnitude_sq()
	local sqrDistance = maxDistanceDelta * maxDistanceDelta

    if sqrDelta > sqrDistance then
		local magnitude = sqrt(sqrDelta)

		if magnitude > 1e-6 then
			delta:mul(maxDistanceDelta / magnitude)
			delta:add(current)
			return delta
		else
			return current:clone()
		end
    end

    return target:clone()
end

function Vector2.scale(a, b)
	local x = a[1] * b[1]
	local y = a[2] * b[2]
	return new_vector2(x, y)
end

function Vector2:equals(other)
	return self[1] == other[1] and self[2] == other[2]
end

function Vector2.reflect(inDirection, inNormal)
	local num = -2 * dot(inNormal, inDirection)
	inNormal = inNormal * num
	inNormal:add(inDirection)
	return inNormal
end

function Vector2.project(vector, onNormal)
	local num = onNormal:magnitude_sq()

	if num < 1.175494e-38 then
		return new_vector2(0,0)
	end

	local num2 = dot(vector, onNormal)
	local v3 = onNormal:clone()
	v3:mul(num2 / num)
	return v3
end

function Vector2:mul(q)
    self[1] = self[1] * q
    self[2] = self[2] * q

	return self
end

function Vector2:div(d)
	self[1] = self[1] / d
	self[2] = self[2] / d

	return self
end

function Vector2:add(vb)
	self[1] = self[1] + vb[1]
	self[2] = self[2] + vb[2]

	return self
end

function Vector2:sub(vb)
	self[1] = self[1] - vb[1]
	self[2] = self[2] - vb[2]

	return self
end

return Vector2

