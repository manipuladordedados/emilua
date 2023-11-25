local xpcall, pcall, scope_push, scope_pop,
    terminate_vm_with_cleanup_error, restore_interruption,
    check_not_interrupted, unpack = ...
return function(f, err, ...)
    check_not_interrupted()
    scope_push()
    local ret = {xpcall(f, err, ...)}
    do
        local cleanup_handlers = scope_pop()
        local i = #cleanup_handlers
        while i > 0 do
            local ok = pcall(cleanup_handlers[i])
            i = i - 1
            if not ok then
                terminate_vm_with_cleanup_error()
            end
        end
        -- scope_pop() already calls disable_interruption()
        restore_interruption()
    end
    return unpack(ret)
end
