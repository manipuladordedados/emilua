-- serialization/good
local spawn_vm = require('./ipc_actor_libspawn').spawn_vm
local inbox = require 'inbox'

if _CONTEXT ~= 'main' then
    local sleep = require('time').sleep

    local ch = inbox:receive()
    sleep(0.1)
    ch:send('localhost')
else
    local my_channel = spawn_vm()

    my_channel:send(inbox)
    print(inbox:receive())
end
