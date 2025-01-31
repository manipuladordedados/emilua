local fs = require 'filesystem'

local ch1_src = {}
ch1_src['a.lua'] = [[
local inbox = require 'inbox'
local ch2 = inbox:receive()
ch2:send('success')
]]

local ch1 = spawn_vm{
    module = fs.path.new('/a.lua'),
    subprocess = {
        pd_daemon = true,
        source_tree_cache = ch1_src,
        stdout = 'share',
        stderr = 'share'
    }
}

local ch2_src = {}
ch2_src['a.lua'] = [[
local inbox = require 'inbox'
print(inbox:receive())
]]

local ch2 = spawn_vm{
    module = fs.path.new('/a.lua'),
    subprocess = {
        pd_daemon = true,
        source_tree_cache = ch2_src,
        stdout = 'share',
        stderr = 'share'
    }
}

ch1:send(ch2)

ch1:detach()
ch2:detach()
