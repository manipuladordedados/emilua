local system = require 'system'
local stream = require 'stream'
local pipe = require 'pipe'
local fs = require 'filesystem'

local pi, po = pipe.pair()

spawn(function()
    stream.write_all(po, 'hello from pipe')
    po:close()
end):detach()

system.spawn{
    program = fs.path.new(system.arguments[1]),
    arguments = { 'emilua', 'spawn_with_pid_on_env_getlowfd.lua' },
    environment = {
        LISTEN_PID = '\0pid',
        LISTEN_FDS = '1'
    },
    stdout = 'share',
    stderr = 'share',
    extra_fds = {
        [3] = pi:release()
    }
}:wait()
