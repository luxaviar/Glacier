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

local Vector3 = {}

local function new_vector3(x, y, z)
	local o = {x or 0, y or 0, z or 0}
	setmetatable(o, Vector3)
	return o
end

setmetatable(Vector3, {
    __call=function(_, x, y, z) return new_vector3(x, y, z) end
})

Vector3.up 		= new_vector3(0,1,0)
Vector3.down 	= new_vector3(0,-1,0)
Vector3.right	= new_vector3(1,0,0)
Vector3.left	= new_vector3(-1,0,0)
Vector3.forward = new_vector3(0,0,1)
Vector3.back	= new_vector3(0,0,-1)
Vector3.zero	= new_vector3(0,0,0)
Vector3.one		= new_vector3(1,1,1)

Vector3.__call = function(_,x,y,z)
    return new_vector3(x, y, z)
end

Vector3.__index = Vector3

Vector3.__tostring = function(self)
    return sformat("[%.3f, %.3f, %.3f]", self[1], self[2], self[3])
end

Vector3.__div = function(va, d)
    return new_vector3(va[1] / d, va[2] / d, va[3] / d)
end

Vector3.__mul = function(va, d)
    if type(d) == "number" then
        return new_vector3(va[1] * d, va[2] * d, va[3] * d)
    else
        local vec = va:clone()
        vec:mul_quat(d)
        return vec
    end
end

Vector3.__add = function(va, vb)
    return new_vector3(va[1] + vb[1], va[2] + vb[2], va[3] + vb[3])
end

Vector3.__sub = function(va, vb)
    return new_vector3(va[1] - vb[1], va[2] - vb[2], va[3] - vb[3])
end

Vector3.__unm = function(va)
    return new_vector3(-va[1], -va[2], -va[3])
end

Vector3.__eq = function(a,b)
    if getmetatable(a) ~= Vector3 or getmetatable(b) ~= Vector3 then
        return false
    end
    
    local v = a - b
    local delta = v:magnitude_sq()
    return delta < 1e-10
end

Vector3.new = new_vector3

function Vector3:x()
    return self[1]
end

function Vector3:y()
    return self[2]
end

function Vector3:z()
    return self[3]
end

function Vector3:set(x,y,z)
	self[1] = x or 0
	self[2] = y or 0
	self[3] = z or 0
end

--function Vector3.get(v)
	--return v[1], v[2], v[3]
--end

function Vector3:clone()
    return new_vector3(self[1], self[2], self[3])
end

function Vector3.distance(va, vb)
	return sqrt((va[1] - vb[1])^2 + (va[2] - vb[2])^2 + (va[3] - vb[3])^2)
end

function Vector3.dot(lhs, rhs)
	return lhs[1] * rhs[1] + lhs[2] * rhs[2] + lhs[3] * rhs[3]
end

function Vector3.lerp(from, to, t)
	t = clamp(t, 0, 1)
	return new_vector3(from[1] + (to[1] - from[1]) * t, from[2] + (to[2] - from[2]) * t, from[3] + (to[3] - from[3]) * t)
end

function Vector3:magnitude_sq()
	local x, y, z = self[1], self[2], self[3]
	return x * x + y * y + z * z
end

function Vector3:magnitude()
	local mag_sq = self:magnitude_sq()
	return sqrt(mag_sq)
end

function Vector3.max(lhs, rhs)
	return new_vector3(max(lhs[1], rhs[1]), max(lhs[2], rhs[2]), max(lhs[3], rhs[3]))
end

function Vector3.min(lhs, rhs)
	return new_vector3(min(lhs[1], rhs[1]), min(lhs[2], rhs[2]), min(lhs[3], rhs[3]))
end

function Vector3:normalized()
	local x, y, z = self[1], self[2], self[3]
	local num = sqrt(x * x + y * y + z * z)

	if num > 1e-5 then
        self[1] = x / num
        self[2] = y / num
        self[3] = z / num
    else
        self[1] = 0
        self[2] = 0
        self[3] = 0
    end
end

function Vector3:normalize()
	local x, y, z = self[1], self[2], self[3]
	local num = sqrt(x * x + y * y + z * z)

	if num > 1e-5 then
        return new_vector3(x / num, y / num, z / num)
    else
        return new_vector3(0, 0, 0)
    end
end

local dot = Vector3.dot

function Vector3.angle(from, to)
	return acos(clamp(dot(from:normalize(), to:normalize()), -1, 1)) * rad2Deg
end

function Vector3:limit(maxLength)
	if self:magnitude_sq() > (maxLength * maxLength) then
		self:normalized()
		self:mul(maxLength)
    end

    return self
end

function Vector3.ortho_normalize(va, vb, vc)
	va:normalized()
	vb:sub(vb:project(va))
	vb:normalized()

	if vc == nil then
		return va, vb
	end

	vc:sub(vc:project(va))
	vc:sub(vc:project(vb))
	vc:normalized()
	return va, vb, vc
end

function Vector3.move_towards(current, target, maxDistanceDelta)
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

function Vector3.scale(a, b)
	local x = a[1] * b[1]
	local y = a[2] * b[2]
	local z = a[3] * b[3]
	return new_vector3(x, y, z)
end

function Vector3.cross(lhs, rhs)
	local x = lhs[2] * rhs[3] - lhs[3] * rhs[2]
	local y = lhs[3] * rhs[1] - lhs[1] * rhs[3]
	local z = lhs[1] * rhs[2] - lhs[2] * rhs[1]
	return new_vector3(x,y,z)
end

function Vector3:equals(other)
	return self[1] == other[1] and self[2] == other[2] and self[3] == other[3]
end

function Vector3.reflect(inDirection, inNormal)
	local num = -2 * dot(inNormal, inDirection)
	inNormal = inNormal * num
	inNormal:add(inDirection)
	return inNormal
end

function Vector3.project(vector, onNormal)
	local num = onNormal:magnitude_sq()

	if num < 1.175494e-38 then
		return new_vector3(0,0,0)
	end

	local num2 = dot(vector, onNormal)
	local v3 = onNormal:clone()
	v3:mul(num2 / num)
	return v3
end

function Vector3.project_plane(vector, planeNormal)
	local v3 = Vector3.project(vector, planeNormal)
	v3:mul(-1)
	v3:add(vector)
	return v3
end

--function Vector3.slerp(from, to, t)
	--local omega, sinom, scale0, scale1

	--if t <= 0 then
		--return from:clone()
	--elseif t >= 1 then
		--return to:clone()
	--end

	--local v2 	= to:clone()
	--local v1 	= from:clone()
	--local len2 	= to:magnitude()
	--local len1 	= from:magnitude()
	--v2:div(len2)
	--v1:div(len1)

	--local len 	= (len2 - len1) * t + len1
	--local cosom = v1[1] * v2[1] + v1[2] * v2[2] + v1[3] * v2[3]

	--if cosom > 1 - 1e-6 then
		--scale0 = 1 - t
		--scale1 = t
	--elseif cosom < -1 + 1e-6 then
		--local axis = ortho_normal_vector(from)
		--local q = Quaternion.angle_axis(180.0 * t, axis)
		--local v = q:mul_vec3(from)
		--v:mul(len)
		--return v
	--else
		--omega 	= acos(cosom)
		--sinom 	= sin(omega)
		--scale0 	= sin((1 - t) * omega) / sinom
		--scale1 	= sin(t * omega) / sinom
	--end

	--v1:mul(scale0)
	--v2:mul(scale1)
	--v2:add(v1)
	--v2:mul(len)
	--return v2
--end

function Vector3:mul(q)
	if type(q) == "number" then
		self[1] = self[1] * q
		self[2] = self[2] * q
		self[3] = self[3] * q
	else
		self:mul_quat(q)
	end

	return self
end

function Vector3:div(d)
	self[1] = self[1] / d
	self[2] = self[2] / d
	self[3] = self[3] / d

	return self
end

function Vector3:add(vb)
	self[1] = self[1] + vb[1]
	self[2] = self[2] + vb[2]
	self[3] = self[3] + vb[3]

	return self
end

function Vector3:sub(vb)
	self[1] = self[1] - vb[1]
	self[2] = self[2] - vb[2]
	self[3] = self[3] - vb[3]

	return self
end

function Vector3:mul_quat(quat)
	local num 	= quat[1] * 2
	local num2 	= quat[2] * 2
	local num3 	= quat[3] * 2
	local num4 	= quat[1] * num
	local num5 	= quat[2] * num2
	local num6 	= quat[3] * num3
	local num7 	= quat[1] * num2
	local num8 	= quat[1] * num3
	local num9 	= quat[2] * num3
	local num10 = quat[4] * num
	local num11 = quat[4] * num2
	local num12 = quat[4] * num3

	local x = (((1 - (num5 + num6)) * self[1]) + ((num7 - num12) * self[2])) + ((num8 + num11) * self[3])
	local y = (((num7 + num12) * self[1]) + ((1 - (num4 + num6)) * self[2])) + ((num9 - num10) * self[3])
	local z = (((num8 - num11) * self[1]) + ((num9 + num10) * self[2])) + ((1 - (num4 + num5)) * self[3])

	self:set(x, y, z)
	return self
end

function Vector3.angle_around_axis (from, to, axis)
	from = from - Vector3.project(from, axis)
	to = to - Vector3.project(to, axis)
	local angle = Vector3.angle(from, to)
	return angle * (Vector3.dot(axis, Vector3.cross (from, to)) < 0 and -1 or 1)
end

return Vector3

