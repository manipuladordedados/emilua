-- serialization/good
local spawn_vm2 = require('./ipc_actor_libspawn').spawn_vm
local sleep = require('time').sleep
local inbox = require 'inbox'

if _CONTEXT ~= 'main' then
    local ch = inbox:receive()
    ch:send('hello')
else
    local container = spawn_vm2('ipc_actor_1_43')
    local actor = spawn_vm('./ipc_actor_1_43_foo')

    actor:send(inbox)
    inbox:receive() --< sync
    sleep(0.1)
    container:send(actor)
    actor:close()
    print(inbox:receive())
end
