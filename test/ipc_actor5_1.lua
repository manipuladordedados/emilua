local stream = require 'stream'
local system = require 'system'
local pipe = require 'pipe'
local fs = require 'filesystem'

local src = {}
src['a.lua'] = [[
local system = require 'system'
local stream = require 'stream'
local sample_plugin_for_lib = require 'sample_plugin_for_lib'

stream.write_all(system.out, sample_plugin_for_lib.foobar() .. '\n')
]]

local guest = spawn_vm{
    module = fs.path.new('/a.lua'),
    subprocess = {
        source_tree_cache = src,
        native_modules_cache = {
            'sample_plugin_for_lib'
        },
        stdout = 'share',
        stderr = 'share'
    }
}

local pi, po = pipe.pair()
po = po:release()

pcall(function() guest:send(po) end)
po:close()
pcall(function()
    local buf = byte_span.new(1)
    pi:read_some(buf)
end)
