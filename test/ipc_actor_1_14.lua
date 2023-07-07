-- serialization/good
local spawn_vm = require('./ipc_actor_libspawn').spawn_vm
local sleep = require('time').sleep
local inbox = require 'inbox'

if _CONTEXT ~= 'main' then
    local msg = inbox:receive()
    local ch = msg.dest
    sleep(0.2)
    ch:send{ value = msg.value }
else
    local my_channel = spawn_vm('ipc_actor_1_14')

    my_channel:send{ dest = inbox, value = 1 / 0 }

    local f = spawn(function()
            inbox:receive()
    end)
    sleep(0.1)
    f:interrupt()
    f:join()

    sleep(0.2)
    print(inbox:receive().value)
end
