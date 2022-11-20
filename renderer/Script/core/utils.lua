local sformat = string.format
local dtraceback = debug.traceback
local LogInfo = require("Glacier.LogInfo")
local LogWarn = require("Glacier.LogWarn")
local LogError = require("Glacier.LogError")

function INFO(fmt, ...)
    local s = sformat(fmt, ...)
    LogInfo(s)
end

function WARNING(fmt,...)
    local s = sformat(fmt, ...)
    LogWarn(s)
end

function ERROR(fmt, ...)
    local tips = dtraceback(nil, 2)
    local str = sformat(fmt,...)
    str = sformat("%s\n%s", str, tips)
    LogError(str)
end