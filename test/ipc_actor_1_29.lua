-- serialization/good
local spawn_vm = require('./ipc_actor_libspawn').spawn_vm
local inbox = require 'inbox'

if _CONTEXT ~= 'main' then
    local sleep = require('time').sleep

    local msg = inbox:receive()
    local ch = msg.dest
    sleep(0.1)
    ch:send(msg.value)
else
    local my_channel = spawn_vm()

    my_channel:send{ dest = inbox, value = 0 }
    print(inbox:receive())
end
