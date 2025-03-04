-- serialization/bad
local spawn_vm = require('./ipc_actor_libspawn').spawn_vm
local badinjector = require 'ipc_actor_badinjector'
local sleep = require('time').sleep

if _CONTEXT ~= 'main' then
    local sleep = require('time').sleep
    local inbox = require 'inbox'

    local f = spawn(function()
        inbox:receive()
    end)
    sleep(0.1)
    f:interrupt()
    f:join()

    sleep(0.2)
    print('RECEIVED:', inbox:receive())
else
    local my_channel = spawn_vm()
    sleep(0.2)
    badinjector.send_overflow_dict(my_channel)
    my_channel:close()
    sleep(0.3) --< wait for some time before we kill the container
end
