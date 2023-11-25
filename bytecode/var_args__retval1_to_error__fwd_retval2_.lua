local error, native = ...
return function(...)
    local e, v = native(...)
    if e then
        error(e, 0)
    end
    return v
end
