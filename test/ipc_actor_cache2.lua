local inbox = require 'inbox'

local child_src = {}

local tmpl = [[
local stream = require 'stream'
local system = require 'system'
local inbox = require 'inbox'

local parent = inbox:receive()
stream.write_all(system.out, '{}\n')
parent:send('exit')
]]

child_src['a.lua'] = format(tmpl, 'a')
child_src['b.lua'] = format(tmpl, 'b')
child_src['c.lua'] = format(tmpl, 'c')
child_src['d.lua'] = format(tmpl, 'd')

local function spwn(mod)
    spawn_vm{
        module = format('/{}.lua', mod),
        subprocess = {
            source_tree_cache = child_src,
            stdout = 'share',
            stderr = 'share'
        }
    }:send(inbox)
    inbox:receive()
end
spwn('a')
spwn('b')
spwn('c')
spwn('d')
