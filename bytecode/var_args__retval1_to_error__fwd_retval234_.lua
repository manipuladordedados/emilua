local error, native = ...
return function(...)
    local e, v1, v2, v3 = native(...)
    if e then
        error(e, 0)
    end
    return v1, v2, v3
end
