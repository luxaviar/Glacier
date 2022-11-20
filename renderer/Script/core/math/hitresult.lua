local tostring = tostring
local sformat = string.format

local RayHitResult = {}

local function new_rayhitresult(hit, t, rigidbody)
	return {hit = hit, t = t, rigidbody = rigidbody}
end

RayHitResult.__index = RayHitResult

RayHitResult.__call = function(hit, t, rigidbody)
    return new_rayhitresult(hit, t, rigidbody)
end

RayHitResult.__tostring = function(self)
	return sformat("RayHitResult [%s, %s, %s]", tostring(self.hit), tostring(self.t), tostring(self.rigidbody))
end

function RayHitResult.new(hit, t, rigidbody)
    return new_rayhitresult(hit, t, rigidbody)
end

return RayHitResult

