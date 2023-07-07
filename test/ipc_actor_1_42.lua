-- serialization/good
local spawn_vm = require('./ipc_actor_libspawn').spawn_vm
local sleep = require('time').sleep
local stream = require 'stream'
local pipe = require 'pipe'

if _CONTEXT ~= 'main' then
    local inbox = require 'inbox'

    local pout = pipe.write_stream.new(inbox:receive().value)
    stream.write_all(pout, 'test')
else
    local pin, pout = pipe.pair()
    pout = pout:release()

    local my_channel = spawn_vm('ipc_actor_1_42')

    sleep(0.1)
    my_channel:send{ value = pout }
    pout:close()
    local buf = byte_span.new(4)
    stream.read_all(pin, buf)
    print(buf)
end
