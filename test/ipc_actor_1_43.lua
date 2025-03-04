-- serialization/good
local system = require 'system'
local sleep = require('time').sleep
local inbox = require 'inbox'

if _CONTEXT ~= 'main' then
    local ch = inbox:receive()
    ch:send('hello')
else
    local container = spawn_vm{
        module = '.',
        subprocess = {
            stdout = 'share',
            stderr = 'share',
            environment = system.environment
        }
    }
    local actor = spawn_vm('./ipc_actor_1_43_foo')

    actor:send(inbox)
    inbox:receive() --< sync
    sleep(0.1)
    container:send(actor)
    actor:close()
    print(inbox:receive())
end
