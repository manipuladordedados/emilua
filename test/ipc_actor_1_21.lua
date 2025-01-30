-- serialization/good
local system = require 'system'
local sleep = require('time').sleep
local inbox = require 'inbox'

if _CONTEXT ~= 'main' then
    local f = spawn(function()
        inbox:receive()
    end)
    sleep(0.1)
    f:cancel()
    f:join()

    sleep(0.2)
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
    local actor = spawn_vm('./ipc_actor_1_21_foo')

    actor:send(inbox)
    inbox:receive() --< sync
    sleep(0.2)
    container:send(actor)
    actor:close()
    print(inbox:receive())
end
