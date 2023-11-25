local io_obj, offset, buffer, minimum = ...
if minimum > #buffer then
    minimum = #buffer
end
local total_nread = 0
while total_nread < minimum do
    local nread = io_obj:read_some_at(offset, buffer)
    offset = offset + nread
    buffer = buffer:slice(1 + nread)
    total_nread = total_nread + nread
end
return total_nread
