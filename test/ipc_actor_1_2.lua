-- serialization/good
local spawn_vm = require('./ipc_actor_libspawn').spawn_vm
local sleep = require('time').sleep
local inbox = require 'inbox'

if _CONTEXT ~= 'main' then
    local ch = inbox:receive()
    sleep(0.2)
    ch:send{ value = 'localhost' }
else
    local my_channel = spawn_vm()

    my_channel:send(inbox)

    local f = spawn(function()
        inbox:receive()
    end)
    sleep(0.1)
    f:interrupt()
    f:join()

    sleep(0.2)
    print(inbox:receive().value)
end
