-- serialization/good
local spawn_vm2 = require('./linux_namespaces_libspawn').spawn_vm
local sleep = require 'sleep'
local inbox = require 'inbox'

local guest_code = [[
    local inbox = require 'inbox'

    local ch = inbox:receive()
    ch:send('hello')
]]

if _CONTEXT == 'main' then
    local container = spawn_vm2(guest_code)
    local actor = spawn_vm('.')

    actor:send(inbox)
    inbox:receive() --< sync
    sleep(0.1)
    container:send(actor)
    actor:close()
    print(inbox:receive())
else
    local master = inbox:receive()
    master:send('started')
    master:send(inbox:receive())
end
