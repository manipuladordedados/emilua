local stream = require 'stream'
local system = require 'system'
local pipe = require 'pipe'
local fs = require 'filesystem'
local unix = require 'unix'

-- We don't support files on FreeBSD yet (waiting for Boost.Asio to add POSIX
-- AIO support). Therefore we use this horrible hack to get the file descriptor
-- for a directory.
if system.arguments[3] == 'gambiarra' then
    local sock = unix.seqpacket.socket.new()
    sock:assign(system.out:dup())
    sock:send_with_fds(byte_span.append('.'), {system.in_.dup()})
    return
end

local library_path_fds
do
    local a, b = unix.seqpacket.socket.pair()
    b = b:release()

    system.spawn{
        program = system.environment.SHELL,
        arguments = {
            system.environment.SHELL,
            '-c',
            format(
                '{emilua:?} {self:?} -- gambiarra <{plugin_path:?}',
                {'emilua', system.arguments[1]},
                {'self', system.arguments[2]},
                {'plugin_path', system.environment.SAMPLE_LIB_FOR_PLUGIN_PATH})
        },
        environment = system.environment,
        stdout = b,
        stderr = 'share'
    }
    b:close()
    library_path_fds = select(2, a:receive_with_fds(byte_span.new(1), 1))
end

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
        ld_library_directories = library_path_fds,
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
