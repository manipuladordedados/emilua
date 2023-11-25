local new_create, new_resume, error, unpack = ...
return function(f)
    local co = new_create(f)
    return function(...)
        local ret = {new_resume(co, ...)}
        if ret[1] == false then
            error(ret[2], 0)
        end
        return unpack(ret, 2)
    end
end
