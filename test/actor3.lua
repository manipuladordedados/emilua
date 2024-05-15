if _CONTEXT ~= 'main' then
    return
end

local ch = spawn_vm('.')
this_fiber.yield();this_fiber.yield();this_fiber.yield();
ch:send('foobar')
