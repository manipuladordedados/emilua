local inbox = require 'inbox'

local master = inbox:receive()
master:send('started')
master:send(inbox:receive())
