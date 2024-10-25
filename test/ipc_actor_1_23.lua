-- serialization/good
local system = require 'system'
local inbox = require 'inbox'

if _CONTEXT ~= 'main' then
    local sleep = require('time').sleep

    local ch = inbox:receive()
    sleep(0.1)
    ch:send('localhost')
else
    local my_channel = spawn_vm{
        module = '.',
        subprocess = {
            stdout = 'share',
            stderr = 'share',
            environment = system.environment
        }
    }

    my_channel:send(inbox)
    print(inbox:receive())
end
