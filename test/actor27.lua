local inbox = require('inbox')

local ch = spawn_vm('./actor27_foo')
ch:send{ from = inbox, body = 'Hello World' }
local m = inbox:receive()
print(m)
