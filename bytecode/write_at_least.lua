local stream, buffer, minimum = ...
if minimum > #buffer then
    minimum = #buffer
end
local total_nwritten = 0
while total_nwritten < minimum do
    local nwritten = stream:write_some(buffer)
    buffer = buffer:slice(1 + nwritten)
    total_nwritten = total_nwritten + nwritten
end
return total_nwritten
