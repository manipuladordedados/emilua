local error, native = ...
return function(cnd, mtx)
    local e = native(cnd, mtx)
    mtx:lock()
    if e then
        error(e, 0)
    end
end
