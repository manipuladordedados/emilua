local error, native, type = ...
return function(chan, ...)
    local e, r = native(chan, ...)
    if e then
        error(e, 0)
    end
    if type(r) == 'function' then
        return r()
    end
    return r
end
