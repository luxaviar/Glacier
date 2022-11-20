local error = error
local sformat = string.format

function assertf(condition, fmt, ...)
    if not condition then
        error(sformat(fmt, ...))
    end
end
