-- serialization/good
local system = require 'system'
local inbox = require 'inbox'

if _CONTEXT ~= 'main' then
    local sleep = require('time').sleep

    local msg = inbox:receive()
    local ch = msg.dest
    sleep(0.1)
    ch:send(msg.value)
else
    local my_channel = spawn_vm{
        module = '.',
        subprocess = {
            stdout = 'share',
            stderr = 'share',
            environment = system.environment
        }
    }

    my_channel:send{ dest = inbox, value = 1 }
    print(inbox:receive())
end
