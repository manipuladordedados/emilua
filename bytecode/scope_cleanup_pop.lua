local scope_cleanup_pop, restore_interruption,
    terminate_vm_with_cleanup_error, pcall = ...
return function(execute)
    if execute == nil then
        execute = true
    end
    local cleanup_handler = scope_cleanup_pop()
    if not execute then
        -- scope_cleanup_pop() already calls disable_interruption()
        restore_interruption()
        return
    end

    local ok = pcall(cleanup_handler)
    if not ok then
        terminate_vm_with_cleanup_error()
    end
    restore_interruption()
end
