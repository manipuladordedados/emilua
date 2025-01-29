local stream, buffer = ...
local ret = #buffer
while #buffer > 0 do
    local nread = stream:read_some(buffer)
    buffer = buffer:sub(1 + nread)
end
return ret
