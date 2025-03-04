local libc_service = require 'libc_service'
local stream = require 'stream'
local unix = require 'unix'
local fs = require 'filesystem'

if _CONTEXT ~= 'main' then
    local s = unix.stream.socket.new()
    s:connect(fs.path.new('/dev/null'))
    s = stream.scanner.new{ stream = s }
    print(s:get_line())
    return
end

local master, slave = libc_service.new()

slave.connect_unix = [[
local real_connect, fd, path = ...
local res, errno, fd2 = real_connect(fd, path)
if fd2 then
    C.dup2(fd2, fd)
    C.close(fd2)
end
return res, errno
]]

spawn_vm{
    module = '.',
    subprocess = {
        libc_service = slave,
        stdout = 'share',
        stderr = 'share',
    }
}

spawn(function() pcall(function()
    while true do
        master:receive()
        if
            master.function_ == 'connect_unix' and
            master:arguments() == fs.path.new('/dev/null')
        then
            local pi, po = unix.stream.socket.pair()
            pi = pi:release()
            spawn(function()
                stream.write_all(po, 'we lied to connect\n')
                po:close()
            end):detach()
            master:send_with_fds(0, {pi})
        else
            master:use_slave_credentials()
        end
    end
end) end):detach()
