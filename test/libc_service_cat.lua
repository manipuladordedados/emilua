local libc_service = require 'libc_service'
local stream = require 'stream'
local system = require 'system'
local pipe = require 'pipe'
local fs = require 'filesystem'

local master, slave = libc_service.new()

slave.open = [[
local real_open, p, f, m = ...
local res, errno, fd = real_open(p, f, m)
if res == -1 then
    return res, errno
else
    return fd
end
]]

spawn(function()
    pcall(function()
        while true do
            master:receive()
            if master.function_ ~= 'open' then
                master:use_slave_credentials()
                goto continue
            end
            local p, f, m = master:arguments()
            if p ~= fs.path.new('/dev/null') then
                master:use_slave_credentials()
                goto continue
            end

            local pi, po = pipe.pair()
            spawn(function()
                stream.write_all(po, 'cat thinks it opened /dev/null\n')
                po:close()
            end):detach()
            pi = pi:release()
            master:send_with_fds(-2, {pi})
            pi:close()
            ::continue::
        end
    end)
end):detach()

local catenv = system.environment
catenv.LD_PRELOAD = catenv.EMILUA_PRELOAD_LIBC_PATH
catenv.EMILUA_LIBC_SERVICE_FD = '3'

system.spawn{
    program = 'cat',
    arguments = { 'cat', '/dev/null' },
    environment = catenv,
    stdout = 'share',
    stderr = 'share',
    extra_fds = {
        [3] = slave,
    },
}:wait()
