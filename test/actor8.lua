-- Test if `inbox:close()` works on active VMs

local sleep = require('time').sleep

if _CONTEXT == 'main' then
    local ch = spawn_vm('.')
    sleep(0.1)
    ch:send('foobar')
else assert(_CONTEXT == 'worker')
    local inbox = require('inbox')
    inbox:close()
    sleep(0.2)
end
