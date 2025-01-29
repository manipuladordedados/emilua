local libc_service = require 'libc_service'
local fs = require 'filesystem'

if _CONTEXT ~= 'main' then
    local status = fs.symlink_status(fs.path.new('/dev/null'))
    print(status.type, status.mode)
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
    if master.function_ == 'lstat' then
        master:send{
            type = 'regular',
            mode = 161,
        }
    else
        master:use_slave_credentials()
    end
end end)
