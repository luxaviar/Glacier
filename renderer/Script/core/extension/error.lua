local error = error
local sformat = string.format

function errorf(fmt, ...)
    error(sformat(fmt, ...))
end

