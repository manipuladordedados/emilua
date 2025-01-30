-- serialization/bad
local system = require 'system'
local badinjector = require 'ipc_actor_badinjector'
local sleep = require('time').sleep

if _CONTEXT ~= 'main' then
    local sleep = require('time').sleep
    local inbox = require 'inbox'

    local f = spawn(function()
        inbox:receive()
    end)
    sleep(0.1)
    f:cancel()
    f:join()

    sleep(0.2)
    print('RECEIVED:', inbox:receive())
else
    local my_channel = spawn_vm{
        module = '.',
        subprocess = {
            stdout = 'share',
            stderr = 'share',
            environment = system.environment
        }
    }
    sleep(0.2)
    badinjector.send_invalid_leaf(my_channel)
    my_channel:close()
    sleep(0.3) --< wait for some time before we kill the container
end
