local inbox = require 'inbox'

local child_src = {}

child_src['app.lua'] = [[
local stream = require 'stream'
local system = require 'system'
local inbox = require 'inbox'

local parent = inbox:receive()
stream.write_all(system.out, 'success\n')
parent:send('exit')
]]

spawn_vm{
    module = '/app.lua',
    subprocess = {
        source_tree_cache = child_src,
        stdout = 'share',
        stderr = 'share'
    }
}:send(inbox)
inbox:receive()
