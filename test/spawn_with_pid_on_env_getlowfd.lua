local system = require 'system'
local stream = require 'stream'
local pipe = require 'pipe'

local has_fd3 = system.environment.LISTEN_PID == tostring(system.getpid())
print(has_fd3)
if has_fd3 and system.environment.LISTEN_FDS == '1' then
    local pi = pipe.read_stream.new()
    pi:assign(system.get_lowfd(3))
    pi = stream.scanner.new{stream = pi}
    print(pi:get_line())
end
