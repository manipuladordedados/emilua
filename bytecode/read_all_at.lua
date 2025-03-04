local io_obj, offset, buffer = ...
local ret = #buffer
while #buffer > 0 do
    local nread = io_obj:read_some_at(offset, buffer)
    offset = offset + nread
    buffer = buffer:slice(1 + nread)
end
return ret
