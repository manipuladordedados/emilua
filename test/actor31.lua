if _CONTEXT ~= 'main' then
    print('worker started')
    return
end

spawn_vm{ module = _FILE }
