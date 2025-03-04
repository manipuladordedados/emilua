if _CONTEXT ~= 'main' then
    local inbox = require 'inbox'
    print(inbox:receive())
    return
end

local guest = spawn_vm{
    module = '.',
    inherit_context = false,
}

guest:send('test')
