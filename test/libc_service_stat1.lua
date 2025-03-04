local libc_service = require 'libc_service'
local fs = require 'filesystem'

if _CONTEXT ~= 'main' then
    print(fs.last_write_time(fs.path.new('/dev/null')).seconds_since_epoch)
    print(fs.hardlink_count(fs.path.new('/dev/null')))
    print(fs.file_size(fs.path.new('/dev/null')))
    local status = fs.status(fs.path.new('/dev/null'))
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
    if master.function_ == 'stat' then
        master:send{
            type = 'regular',
            mode = 161,
            mtime = fs.clock.epoch(),
            nlink = 42,
            size = 111,
        }
    else
        master:use_slave_credentials()
    end
end end)
