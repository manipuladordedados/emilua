local error, unpack, native = ...
return function(...)
    local args = {native(...)}
    if args[1] then
        return unpack(args, 2)
    else
        error(args[2], 0)
    end
end
