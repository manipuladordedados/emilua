local stream, buffer, minimum = ...
if minimum > #buffer then
    minimum = #buffer
end
local total_nread = 0
while total_nread < minimum do
    local nread = stream:read_some(buffer)
    buffer = buffer:slice(1 + nread)
    total_nread = total_nread + nread
end
return total_nread
