-- close() doesn't discard data already sent
local spawn_vm = require('./ipc_actor_libspawn').spawn_vm
local sleep = require('time').sleep

if _CONTEXT ~= 'main' then
    local inbox = require 'inbox'
    print(inbox:receive())
else
    local my_channel = spawn_vm()
    my_channel:send('hello')
    my_channel:close()
    sleep(0.3) --< wait for some time before we kill the container
end
