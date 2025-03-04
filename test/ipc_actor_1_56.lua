-- close() doesn't discard data already sent
local system = require 'system'
local sleep = require('time').sleep

if _CONTEXT ~= 'main' then
    local inbox = require 'inbox'
    print(inbox:receive())
else
    local my_channel = spawn_vm{
        module = '.',
        subprocess = {
            stdout = 'share',
            stderr = 'share',
            environment = system.environment
        }
    }
    my_channel:send('hello')
    my_channel:close()
    sleep(0.3) --< wait for some time before we kill the container
end
