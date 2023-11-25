local resume, yield, is_yield_native,
    mark_yield_as_native, is_busy, set_busy,
    clear_busy, eperm, check_not_interrupted,
    error, unpack = ...
return function(co, ...)
    check_not_interrupted()

    if is_busy(co) then
        error(eperm, 0)
    end

    local args = {...}
    while true do
        local ret = {resume(co, unpack(args))}
        if ret[1] == false then
            mark_yield_as_native()
            return unpack(ret)
        end
        if is_yield_native() then
            set_busy(co)
            args = {yield(unpack(ret, 2))}
            clear_busy(co)
        else
            mark_yield_as_native()
            return unpack(ret)
        end
    end
end
