local scope_push, scope_pop, terminate_vm_with_cleanup_error,
    restore_interruption, old_pcall, error = ...
return function(f)
    scope_push()
    local ok, e = old_pcall(f)
    do
        local cleanup_handlers = scope_pop()
        local i = #cleanup_handlers
        while i > 0 do
            local ok = old_pcall(cleanup_handlers[i])
            i = i - 1
            if not ok then
                terminate_vm_with_cleanup_error()
            end
        end
        -- scope_pop() already calls disable_interruption()
        restore_interruption()
    end
    if not ok then
        error(e, 0)
    end
end
