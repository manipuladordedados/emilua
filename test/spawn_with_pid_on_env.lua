local system = require 'system'
local stream = require 'stream'
local pipe = require 'pipe'
local fs = require 'filesystem'

local pi, po = pipe.pair()

-- subprocess.wait() isn't implemented for all platforms (e.g. macOS), so we use
-- EPIPE to wait for the child
local exit_pi, exit_po = pipe.pair()
exit_po = exit_po:release()

spawn(function()
    stream.write_all(po, 'hello from pipe')
    po:close()
end):detach()

local p_env = system.environment
p_env.LISTEN_PID = '\0pid'
p_env.LISTEN_FDS = '1'
system.spawn{
    program = fs.path.new(system.arguments[1]),
    arguments = { 'emilua', 'spawn_with_pid_on_env_getlowfd.lua' },
    environment = p_env,
    stdout = 'share',
    stderr = 'share',
    extra_fds = {
        [3] = pi:release(),
        [4] = exit_po
    }
}
exit_po:close()

pcall(function()
    local buf = byte_span.new(1)
    exit_pi:read_some(buf)
end)
