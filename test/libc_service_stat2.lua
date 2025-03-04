local libc_service = require 'libc_service'
local errno = require 'generic_error'
local fs = require 'filesystem'

if _CONTEXT ~= 'main' then
    print(fs.hardlink_count(fs.path.new('/dev/null')))
    return
end

local master, slave = libc_service.new()

spawn_vm{
    module = '.',
    subprocess = {
        stdout = 'share',
        stderr = 'share',
        libc_service = slave,
    }
}

pcall(function() while true do
    master:receive()
    if master.function_ == 'stat' then
        master:send(-1, errno.EDEADLK)
    else
        master:use_slave_credentials()
    end
end end)
