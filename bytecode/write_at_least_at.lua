local io_obj, offset, buffer, minimum = ...
if minimum > #buffer then
    minimum = #buffer
end
local total_nwritten = 0
while total_nwritten < minimum do
    local nwritten = io_obj:write_some_at(offset, buffer)
    offset = offset + nwritten
    buffer = buffer:slice(1 + nwritten)
    total_nwritten = total_nwritten + nwritten
end
return total_nwritten
