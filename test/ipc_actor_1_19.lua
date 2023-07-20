-- serialization/good
local spawn_vm = require('./ipc_actor_libspawn').spawn_vm
local sleep = require('time').sleep
local stream = require 'stream'
local pipe = require 'pipe'

if _CONTEXT ~= 'main' then
    local inbox = require 'inbox'

    local f = spawn(function()
        inbox:receive()
    end)
    sleep(0.1)
    f:interrupt()
    f:join()

    sleep(0.2)
    local pout = pipe.write_stream.new(inbox:receive())
    stream.write_all(pout, 'test')
else
    local pin, pout = pipe.pair()
    pout = pout:release()

    local my_channel = spawn_vm()

    sleep(0.2)
    my_channel:send(pout)
    pout:close()
    local buf = byte_span.new(4)
    stream.read_all(pin, buf)
    print(buf)
end
