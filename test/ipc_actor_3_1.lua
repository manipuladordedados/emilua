local system = require 'system'
local stream = require 'stream'
local inbox = require 'inbox'

if _CONTEXT ~= 'main' then
    pcall(function() stream.write_all(system.out, 'garbage\n') end)
    local ch = inbox:receive()
    ch:send('localhost')
else
    local my_channel = spawn_vm{
        module = '.',
        subprocess = {
            stderr = 'share',
            environment = system.environment
        }
    }

    my_channel:send(inbox)
    print(inbox:receive())
end
