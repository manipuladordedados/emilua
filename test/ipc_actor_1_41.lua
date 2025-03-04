-- serialization/good
local system = require 'system'
local sleep = require('time').sleep
local stream = require 'stream'
local pipe = require 'pipe'

if _CONTEXT ~= 'main' then
    local inbox = require 'inbox'

    local pout = pipe.write_stream.new(inbox:receive())
    stream.write_all(pout, 'test')
else
    local pin, pout = pipe.pair()
    pout = pout:release()

    local my_channel = spawn_vm{
        module = '.',
        subprocess = {
            stdout = 'share',
            stderr = 'share',
            environment = system.environment
        }
    }

    sleep(0.1)
    my_channel:send(pout)
    pout:close()
    local buf = byte_span.new(4)
    stream.read_all(pin, buf)
    print(buf)
end
