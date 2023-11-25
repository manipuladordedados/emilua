local root_scope, set_current_traceback,
    terminate_vm_with_cleanup_error, xpcall,
    pcall, error, unpack = ...
return function(start_fn)
    return function()
        local ret = {xpcall(
            start_fn,
            function(e)
                set_current_traceback()
                return e
            end
        )}
        do
            local cleanup_handlers = root_scope()
            local i = #cleanup_handlers
            while i > 0 do
                local ok = pcall(cleanup_handlers[i])
                i = i - 1
                if not ok then
                    terminate_vm_with_cleanup_error()
                end
            end
        end
        if ret[1] == false then
            error(ret[2], 0)
        end
        return unpack(ret, 2)
    end
end
