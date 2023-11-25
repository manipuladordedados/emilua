local error, native = ...
return function(...)
    local e = native(...)
    if e then
        error(e, 0)
    end
end
