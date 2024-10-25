local inbox = require 'inbox'
local fs = require 'filesystem'

local child_src = {}

local function spwn(mod)
    spawn_vm{
        module = fs.path.new(format('/{}.lua', mod)),
        subprocess = {
            source_tree_cache = child_src,
            stdout = 'share',
            stderr = 'share'
        }
    }:send(inbox)
    inbox:receive()
end

local tmpl = [[
local stream = require 'stream'
local system = require 'system'
local inbox = require 'inbox'

local parent = inbox:receive()
stream.write_all(system.out, '{}\n')
parent:send('exit')
]]

child_src['a.lua'] = format(tmpl, 'a')
child_src['b'] = {
    ['init.lua'] = format(tmpl, 'b')
}
child_src['c'] = {
    d = {
        ['init.lua'] = format(tmpl, 'c/d')
    },
    ['e.lua'] = format(tmpl, 'c/e')
}
child_src['d'] = {
    e = {
        f = {
            g = {
                ['init.lua'] = format(tmpl, 'd/e/f/g')
            },
            ['h.lua'] = format(tmpl, 'd/e/f/h')
        },
        g = {
            ['h.lua'] = format(tmpl, 'd/e/g/h'),
            ['i.lua'] = format(tmpl, 'd/e/g/i')
        },
        ['h.lua'] = format(tmpl, 'd/e/h')
    },
    ['f.lua'] = format(tmpl, 'd/f')
}

spwn('a')
spwn('b/init')
spwn('c/d/init')
spwn('c/e')
spwn('d/e/f/g/init')
spwn('d/e/f/h')
spwn('d/e/g/h')
spwn('d/e/g/i')
spwn('d/e/h')
spwn('d/f')
