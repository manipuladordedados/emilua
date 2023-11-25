local root_scope, terminate_vm_with_cleanup_error,
    pcall, error, mark_module_as_loaded,
    start_fn = ...
return function()
    local ok, e = pcall(start_fn)
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
    if ok == false then
        error(e, 0)
    end
    return mark_module_as_loaded()
end
