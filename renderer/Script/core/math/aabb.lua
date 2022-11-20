local math	= math
local mmin	= math.min
local mmax 	= math.max
local huge  = math.huge
local setmetatable = setmetatable
local tostring = tostring
local Vector3 = require("core.math.vector3")
local sformat = string.format
local new_vector3 = Vector3.new

local AABB = {}

-- luacheck: ignore self
local function new_aabb(min, max)
	local o = {min=min, max=max}
	setmetatable(o, AABB)
	return o
end

AABB.__index = AABB

AABB.__call = function(min, max)
    return new_aabb(min, max)
end

AABB.__tostring = function(self)
	return sformat("AABB [%s, %s]", tostring(self.min), tostring(self.max))
end

function AABB.new_ex(x1, y1, z1, x2, y2, z2)
    return new_aabb(new_vector3(x1, y1, z1), new_vector3(x2, y2, z2))
end

function AABB.new(min, max)
    return new_aabb(min, max)
end

function AABB:center()
    return (self.min + self.max) * 0.5
end

function AABB:size()
    return self.max - self.min
end

function AABB:extens()
    return self:size() * 0.5
end

function AABB:area()
    local max = self.max
    local min = self.min
    return 2.0 * ((max[1] - min[1]) * (max[2] - min[2]) + (max[2] - min[2]) * (max[3] - min[3]) + (max[3] - min[3]) * (max[1] - min[1]));
end

function AABB:volume()
    local size = self.max - self.min;
    return (size[1] * size[2] * size[3]);
end

function AABB:closest_point(point)
    local smin = self.min
    local smax = self.max
    local x = mmax(smin[1], mmin(point[1], smax[1]));
    local y = mmax(smin[2], mmin(point[2], smax[2]));
    local z = mmax(smin[3], mmin(point[3], smax[3]));
    return new_vector3(x, y, z);
end

function AABB:contains(other)
    local min = self.min
    local max = self.max
    local omin = other.min
    local omax = other.max

    return min[1] < omin[1] and min[2] < omin[2] and min[3] < omin[3] and
        max[1] > omax[1] and max[2] > omax[2] and max[3] > omax[3];
end

function AABB:contains_point(point)
    local min = self.min
    local max = self.max
    return point[1] >= min[1] and point[1] <= max[1] and
        point[2] >= min[2] and point[2] <= max[2] and
        point[3] >= min[3] and point[3] <= max[3];
end

function AABB:contains_sphere(center, radius)
    if not self:contains_point(center) then
        return false;
    end

    local smin = self.min
    local smax = self.max
    local x = mmin(center[1] - smin[1], smax[1] - center[1]);
    local y = mmin(center[2] - smin[2], smax[2] - center[2]);
    local z = mmin(center[3] - smin[3], smax[3] - center[3]);
    local dist = mmin(x, mmin(y, z));
    return dist > radius;
end

function AABB:intersects(other)
    local min = self.min
    local max = self.max
    local omin = other.min
    local omax = other.max
    return min[1] < omax[1] and max[1] > omin[1] and
        min[2] < omax[2] and max[2] > omin[2] and
        min[3] < omax[3] and max[3] > omin[3];
end


function AABB:intersects_inclusive(other)
    local min = self.min
    local max = self.max
    local omin = other.min
    local omax = other.max
    return min[1] <= omax[1] and max[1] >= omin[1] and
        min[2] <= omax[2] and max[2] >= omin[2] and
        min[3] <= omax[3] and max[3] >= omin[3];
end

function AABB:intersects_ray(ray, maxDistance)
    local tmin = -huge
    local tmax = huge

    local direction = {ray.direction[1], ray.direction[2], ray.direction[3]}
    local origin = {ray.origin[1], ray.origin[2], ray.origin[3]}

    for i = 1, 3 do
        local invD = 1.0 / direction[i] --TODO: can compiler correct handle divided by zero? we expect inf result;
        local t0 = (self.min[i] - origin[i]) * invD
        local t1 = (self.max[i] - origin[i]) * invD
        if invD < 0.0 then
            local temp = t1
            t1 = t0
            t0 = temp
        end

        if t0 > tmin then
            tmin = t0
        end

        if t1 < tmax then
            tmax = t1
        end

        if tmax < tmin or tmax < 0.0 then
            return false
        end
    end

    if maxDistance > 0.0 and tmin > maxDistance then
        return false
    end

    return true, tmin
end

function AABB:distance(point)
    local p = self:closest_point(point)
    return p:distance(point)
end

return AABB

