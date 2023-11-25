local error, native = ...
return function(module)
    local ok, val = native(module)
    if ok then
        return val
    else
        error(val, 0)
    end
end
