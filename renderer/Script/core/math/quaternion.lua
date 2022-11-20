local math	= math
local sin 	= math.sin
local cos 	= math.cos
local acos 	= math.acos
local asin 	= math.asin
local sqrt 	= math.sqrt
local min	= math.min
local atan2 = math.atan
local clamp = math.clamp
local abs	= math.abs
local setmetatable = setmetatable
local getmetatable = getmetatable
local rawset = rawset
local Vector3 = require("core.math.vector3")

local rad2Deg = 57.295779513082
local deg2Rad = 0.017453292519943
local halfDegToRad = 0.5 * deg2Rad

local _forward = Vector3.forward
local _up = Vector3.up
local _next = { 2, 3, 1 }

local Quaternion = {}

local function new_quaternion(x, y, z, w)
	local o = {x or 0, y or 0, z or 0, w or 1}
	setmetatable(o, Quaternion)
	return o
end

setmetatable(Quaternion, {
    __call=function(_, x, y, z, w) return new_quaternion(x, y, z, w) end
})

Quaternion.__index = Quaternion

Quaternion.__newindex = function(t, name, k)
	if name == "eulerAngles" then
		t:set_euler(k)
	else
		rawset(t, name, k)
	end
end

Quaternion.__call = function(_, x, y, z, w)
    return new_quaternion(x, y, z, w)
end

Quaternion.__mul = function(lhs, rhs)
	if Quaternion == getmetatable(rhs) then
		return new_quaternion((((lhs[4] * rhs[1]) + (lhs[1] * rhs[4])) + (lhs[2] * rhs[3])) - (lhs[3] * rhs[2]),
            (((lhs[4] * rhs[2]) + (lhs[2] * rhs[4])) + (lhs[3] * rhs[1])) - (lhs[1] * rhs[3]),
            (((lhs[4] * rhs[3]) + (lhs[3] * rhs[4])) + (lhs[1] * rhs[2])) - (lhs[2] * rhs[1]),
            (((lhs[4] * rhs[4]) - (lhs[1] * rhs[1])) - (lhs[2] * rhs[2])) - (lhs[3] * rhs[3]))
	elseif Vector3 == getmetatable(rhs) then
		return lhs:mul_vec3(rhs)
	end
end

Quaternion.__unm = function(q)
	return new_quaternion(-q[1], -q[2], -q[3], -q[4])
end

Quaternion.__eq = function(lhs,rhs)
    if getmetatable(lhs) ~= Quaternion or getmetatable(rhs) ~= Quaternion then
        return false
    end
	return Quaternion.dot(lhs, rhs) > 0.999999
end

Quaternion.__tostring = function(self)
	return "["..self[1]..","..self[2]..","..self[3]..","..self[4].."]"
end

function Quaternion.new(x, y, z, w)
    return new_quaternion(x, y, z, w)
end

function Quaternion:x()
    return self[1]
end

function Quaternion:y()
    return self[2]
end

function Quaternion:z()
    return self[3]
end

function Quaternion:w()
    return self[4]
end

function Quaternion:set(x,y,z,w)
	self[1] = x or 0
	self[2] = y or 0
	self[3] = z or 0
	self[4] = w or 0
end

function Quaternion:clone()
	return new_quaternion(self[1], self[2], self[3], self[4])
end

--function Quaternion:get()
	--return self[1], self[2], self[3], self[4]
--end

function Quaternion.dot(a, b)
	return a[1] * b[1] + a[2] * b[2] + a[3] * b[3] + a[4] * b[4]
end

function Quaternion.angle(a, b)
	local dot = Quaternion.dot(a, b)
	if dot < 0 then dot = -dot end
	return acos(min(dot, 1)) * 2 * 57.29578
end

function Quaternion.angle_axis(angle, axis)
	local normAxis = axis:normalize()
    angle = angle * halfDegToRad
    local s = sin(angle)

    local w = cos(angle)
    local x = normAxis[1] * s
    local y = normAxis[2] * s
    local z = normAxis[3] * s

	return new_quaternion(x,y,z,w)
end

function Quaternion.equals(a, b)
	return a[1] == b[1] and a[2] == b[2] and a[3] == b[3] and a[4] == b[4]
end

function Quaternion.euler(x, y, z)
	if y == nil and z == nil then
		y = x[2]
		z = x[3]
		x = x[1]
	end

	x = x * 0.0087266462599716
    y = y * 0.0087266462599716
    z = z * 0.0087266462599716

	local sinX = sin(x)
    x = cos(x)
    local sinY = sin(y)
    y = cos(y)
    local sinZ = sin(z)
    z = cos(z)

    local q = {
        [1] = y * sinX * z + sinY * x * sinZ,
        [2] = sinY * x * z - y * sinX * sinZ,
        [3] = y * x * sinZ - sinY * sinX * z,
        [4] = y * x * z + sinY * sinX * sinZ
    }
	setmetatable(q, Quaternion)
	return q
end

function Quaternion:set_euler(x, y, z)
	if y == nil and z == nil then
		y = x[2]
		z = x[3]
		x = x[1]
	end

	x = x * 0.0087266462599716
    y = y * 0.0087266462599716
    z = z * 0.0087266462599716

	local sinX = sin(x)
    local cosX = cos(x)
    local sinY = sin(y)
    local cosY = cos(y)
    local sinZ = sin(z)
    local cosZ = cos(z)

    self[4] = cosY * cosX * cosZ + sinY * sinX * sinZ
    self[1] = cosY * sinX * cosZ + sinY * cosX * sinZ
    self[2] = sinY * cosX * cosZ - cosY * sinX * sinZ
    self[3] = cosY * cosX * sinZ - sinY * sinX * cosZ

	return self
end

function Quaternion:normalize()
	local quat = self:Clone()
	quat:normalized()
	return quat
end

function Quaternion:normalized()
	local n = self[1] * self[1] + self[2] * self[2] + self[3] * self[3] + self[4] * self[4]

	if n ~= 1 and n > 0 then
		n = 1 / sqrt(n)
		self[1] = self[1] * n
		self[2] = self[2] * n
		self[3] = self[3] * n
		self[4] = self[4] * n
	end
end

local function MatrixToQuaternion(rot)
	local trace = rot[1][1] + rot[2][2] + rot[3][3]

	if trace > 0 then
		local s = sqrt(trace + 1)
		local w = 0.5 * s
		s = 0.5 / s
        local quat = new_quaternion(
            (rot[3][2] - rot[2][3]) * s,
            (rot[1][3] - rot[3][1]) * s,
            (rot[2][1] - rot[1][2]) * s,
            w
        )
		quat:normalized()
        return quat
	else
		local i = 1
		local q = {0, 0, 0}

		if rot[2][2] > rot[1][1] then
			i = 2
		end

		if rot[3][3] > rot[i][i] then
			i = 3
		end

		local j = _next[i]
		local k = _next[j]

		local t = rot[i][i] - rot[j][j] - rot[k][k] + 1
		local s = 0.5 / sqrt(t)
		q[i] = s * t
		local w = (rot[k][j] - rot[j][k]) * s
		q[j] = (rot[j][i] + rot[i][j]) * s
		q[k] = (rot[k][i] + rot[i][k]) * s

		local quat = new_quaternion(q[1], q[2], q[3], w)
		quat:normalized()
        return quat
	end
end

function Quaternion.fromto_rotation(from, to)
	--from = from:normalize()
	--to = to:normalize()

	local e = Vector3.dot(from, to)

	if e > 1 - 1e-6 then
		return new_quaternion(0, 0, 0, 1)
	elseif e < -1 + 1e-6 then
		local left = {0, from[3], from[2]}
		local mag = left[2] * left[2] + left[3] * left[3]  --+ left[1] * left[1] = 0

		if mag < 1e-6 then
			left[1] = -from[3]
			left[2] = 0
			left[3] = from[1]
			mag = left[1] * left[1] + left[3] * left[3]
		end

		local invlen = 1/sqrt(mag)
		left[1] = left[1] * invlen
		left[2] = left[2] * invlen
		left[3] = left[3] * invlen

		local up = {0, 0, 0}
		up[1] = left[2] * from[3] - left[3] * from[2]
		up[2] = left[3] * from[1] - left[1] * from[3]
		up[3] = left[1] * from[2] - left[2] * from[1]


		local fxx = -from[1] * from[1]
		local fyy = -from[2] * from[2]
		local fzz = -from[3] * from[3]

		local fxy = -from[1] * from[2]
		local fxz = -from[1] * from[3]
		local fyz = -from[2] * from[3]

		local uxx = up[1] * up[1]
		local uyy = up[2] * up[2]
		local uzz = up[3] * up[3]
		local uxy = up[1] * up[2]
		local uxz = up[1] * up[3]
		local uyz = up[2] * up[3]

		local lxx = -left[1] * left[1]
		local lyy = -left[2] * left[2]
		local lzz = -left[3] * left[3]
		local lxy = -left[1] * left[2]
		local lxz = -left[1] * left[3]
		local lyz = -left[2] * left[3]

		local rot =
		{
			{fxx + uxx + lxx, fxy + uxy + lxy, fxz + uxz + lxz},
			{fxy + uxy + lxy, fyy + uyy + lyy, fyz + uyz + lyz},
			{fxz + uxz + lxz, fyz + uyz + lyz, fzz + uzz + lzz},
		}

		return MatrixToQuaternion(rot)
	else
		local v = Vector3.Cross(from, to)
		local h = (1 - e) / Vector3.Dot(v, v)

		local hx = h * v[1]
		local hz = h * v[3]
		local hxy = hx * v[2]
		local hxz = hx * v[3]
		local hyz = hz * v[2]

		local rot =
		{
			{e + hx*v[1], 	hxy - v[3], 		hxz + v[2]},
			{hxy + v[3],  	e + h*v[2]*v[2], 	hyz-v[1]},
			{hxz - v[2],  	hyz + v[1],    	e + hz*v[3]},
		}

		return MatrixToQuaternion(rot)
	end
end

function Quaternion:inversed()
	local quat = Quaternion.new()

	quat[1] = -self[1]
	quat[2] = -self[2]
	quat[3] = -self[3]
	quat[4] = self[4]

	return quat
end

function Quaternion.lerp(q1, q2, t)
	t = clamp(t, 0, 1)
	local q = {x = 0, y = 0, z = 0, w = 1}

	if Quaternion.dot(q1, q2) < 0 then
		q[1] = q1[1] + t * (-q2[1] -q1[1])
		q[2] = q1[2] + t * (-q2[2] -q1[2])
		q[3] = q1[3] + t * (-q2[3] -q1[3])
		q[4] = q1[4] + t * (-q2[4] -q1[4])
	else
		q[1] = q1[1] + (q2[1] - q1[1]) * t
		q[2] = q1[2] + (q2[2] - q1[2]) * t
		q[3] = q1[3] + (q2[3] - q1[3]) * t
		q[4] = q1[4] + (q2[4] - q1[4]) * t
	end

	setmetatable(q, Quaternion)
	q:normalized()
	return q
end

function Quaternion.look_rotation(forward, up)
	local mag = forward:magnitude()
	if mag < 1e-6 then
		error("error input forward to Quaternion.LookRotation"..tostring(forward))
		return nil
	end

	forward = forward / mag
	up = up or _up
	local right = Vector3.cross(up, forward)
	right:normalized()
    up = Vector3.cross(forward, right)
    right = Vector3.cross(up, forward)

--[[	local quat = new_quaternion(0,0,0,1)
	local rot =
	{
		{right[1], up[1], forward[1]},
		{right[2], up[2], forward[2]},
		{right[3], up[3], forward[3]},
	}

	MatrixToQuaternion(rot, quat)
	return quat--]]

    local t = right[1] + up[2] + forward[3]

	if t > 0 then
		local x, y, z, w
		t = t + 1
		local s = 0.5 / sqrt(t)
		w = s * t
		x = (up[3] - forward[2]) * s
		y = (forward[1] - right[3]) * s
		z = (right[2] - up[1]) * s

		local ret = new_quaternion(x, y, z, w)
		ret:normalized()
		return ret
	else
		local rot =
		{
			{right[1], up[1], forward[1]},
			{right[2], up[2], forward[2]},
			{right[3], up[3], forward[3]},
		}

		local q = {0, 0, 0}
		local i = 1

		if up[2] > right[1] then
			i = 2
		end

		if forward[3] > rot[i][i] then
			i = 3
		end

		local j = _next[i]
		local k = _next[j]

		t = rot[i][i] - rot[j][j] - rot[k][k] + 1
		local s = 0.5 / sqrt(t)
		q[i] = s * t
		local w = (rot[k][j] - rot[j][k]) * s
		q[j] = (rot[j][i] + rot[i][j]) * s
		q[k] = (rot[k][i] + rot[i][k]) * s

		local ret = new_quaternion(q[1], q[2], q[3], w)
		ret:normalized()
		return ret
	end
end

local function unclamped_slerp(q1, q2, t)
	local dot = q1[1] * q2[1] + q1[2] * q2[2] + q1[3] * q2[3] + q1[4] * q2[4]

    if dot < 0 then
        dot = -dot
        q2 = setmetatable({x = -q2[1], y = -q2[2], z = -q2[3], w = -q2[4]}, Quaternion)
    end

    if dot < 0.95 then
	    local angle = acos(dot)
        local invSinAngle = 1 / sin(angle)
        local t1 = sin((1 - t) * angle) * invSinAngle
        local t2 = sin(t * angle) * invSinAngle
        q1 = {q1[1] * t1 + q2[1] * t2, q1[2] * t1 + q2[2] * t2, q1[3] * t1 + q2[3] * t2, q1[4] * t1 + q2[4] * t2}
		setmetatable(q1, Quaternion)
		return q1
	else
		q1 = {q1[1] + t * (q2[1] - q1[1]), q1[2] + t * (q2[2] - q1[2]), q1[3] + t * (q2[3] - q1[3]), q1[4] + t * (q2[4] - q1[4])}
		setmetatable(q1, Quaternion)
        q1:normalized()
		return q1
    end
end


function Quaternion.slerp(from, to, t)
	if t < 0 then
		t = 0
	elseif t > 1 then
		t = 1
	end

	return unclamped_slerp(from, to, t)
end

function Quaternion.rotate_towards(from, to, maxDegreesDelta)
	local angle = Quaternion.angle(from, to)

	if angle == 0 then
		return to
	end

	local t = min(1, maxDegreesDelta / angle)
	return unclamped_slerp(from, to, t)
end

local function approximately(f0, f1)
	return abs(f0 - f1) < 1e-6
end

function Quaternion:to_angle_axis()
	local angle = 2 * acos(self[4])

	if approximately(angle, 0) then
		return angle * 57.29578, Vector3.new(1, 0, 0)
	end

	local div = 1 / sqrt(1 - sqrt(self[4]))
	return angle * 57.29578, Vector3.new(self[1] * div, self[2] * div, self[3] * div)
end

local pi = math.pi
local half_pi = pi * 0.5
local two_pi = 2 * pi
local negativeFlip = -0.0001
local positiveFlip = two_pi - 0.0001

local function SanitizeEuler(euler)
	if euler[1] < negativeFlip then
		euler[1] = euler[1] + two_pi
	elseif euler[1] > positiveFlip then
		euler[1] = euler[1] - two_pi
	end

	if euler[2] < negativeFlip then
		euler[2] = euler[2] + two_pi
	elseif euler[2] > positiveFlip then
		euler[2] = euler[2] - two_pi
	end

	if euler[3] < negativeFlip then
		euler[3] = euler[3] + two_pi
	elseif euler[3] > positiveFlip then
		euler[3] = euler[3] + two_pi
	end
end

--from http://www.geometrictools.com/Documentation/EulerAngles.pdf
--Order of rotations: YXZ
function Quaternion:to_euler_angles()
	local x = self[1]
	local y = self[2]
	local z = self[3]
	local w = self[4]

	local check = 2 * (y * z - w * x)

	if check < 0.999 then
		if check > -0.999 then
			local v = Vector3.new( -asin(check),
						atan2(2 * (x * z + w * y), 1 - 2 * (x * x + y * y)),
						atan2(2 * (x * y + w * z), 1 - 2 * (x * x + z * z)))
			SanitizeEuler(v)
			v:mul(rad2Deg)
			return v
		else
			local v = Vector3.new(half_pi, atan2(2 * (x * y - w * z), 1 - 2 * (y * y + z * z)), 0)
			SanitizeEuler(v)
			v:mul(rad2Deg)
			return v
		end
	else
		local v = Vector3.new(-half_pi, atan2(-2 * (x * y - w * z), 1 - 2 * (y * y + z * z)), 0)
		SanitizeEuler(v)
		v:mul(rad2Deg)
		return v
	end
end

function Quaternion:forward()
	return self:mul_vec3(_forward)
end

function Quaternion:mul_vec3(point)
	local vec = Vector3.new()

	local num 	= self[1] * 2
	local num2 	= self[2] * 2
	local num3 	= self[3] * 2
	local num4 	= self[1] * num
	local num5 	= self[2] * num2
	local num6 	= self[3] * num3
	local num7 	= self[1] * num2
	local num8 	= self[1] * num3
	local num9 	= self[2] * num3
	local num10 = self[4] * num
	local num11 = self[4] * num2
	local num12 = self[4] * num3

	vec[1] = (((1 - (num5 + num6)) * point[1]) + ((num7 - num12) * point[2])) + ((num8 + num11) * point[3])
	vec[2] = (((num7 + num12) * point[1]) + ((1 - (num4 + num6)) * point[2])) + ((num9 - num10) * point[3])
	vec[3] = (((num8 - num11) * point[1]) + ((num9 + num10) * point[2])) + ((1 - (num4 + num5)) * point[3])

	return vec
end

Quaternion.identity = new_quaternion(0, 0, 0, 1)
Quaternion.euler_angles = Quaternion.to_euler_angles

return Quaternion

