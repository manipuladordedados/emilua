local libc_service = require 'libc_service'
local unix = require 'unix'
local fs = require 'filesystem'

if _CONTEXT ~= 'main' then
    local s = unix.datagram.socket.new()
    s:open()
    s:bind(fs.path.new('/dev/null'))
    local buf = byte_span.new(1024)
    local nread = s:receive(buf)
    print(buf:first(nread))
    return
end

local master, slave = libc_service.new()

slave.bind_unix = [[
local real_bind, fd, path = ...
local res, errno, fd2 = real_bind(fd, path)
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
            master.function_ == 'bind_unix' and
            master:arguments() == fs.path.new('/dev/null')
        then
            local pi, po = unix.datagram.socket.pair()
            pi = pi:release()
            spawn(function()
                po:send(byte_span.append('we lied to bind'))
                po:close()
            end):detach()
            master:send_with_fds(0, {pi})
        else
            master:use_slave_credentials()
        end
    end
end) end):detach()
