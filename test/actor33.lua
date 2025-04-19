if _CONTEXT ~= 'main' then
    spawn_context_threads(1)
    return
end

spawn_vm{
    module = '.',
    new_master = true
}
