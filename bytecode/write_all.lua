local type, byte_span_append = ...
return function(stream, buffer)
    local ret = #buffer
    if type(buffer) == 'string' then
        buffer = byte_span_append(buffer)
    end
    while #buffer > 0 do
        local nwritten = stream:write_some(buffer)
        buffer = buffer:slice(1 + nwritten)
    end
    return ret
end
