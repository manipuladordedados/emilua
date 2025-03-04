local libc_service = require 'libc_service'
local fs = require 'filesystem'

if _CONTEXT ~= 'main' then
    print(fs.hardlink_count(fs.path.new('/dev/null')))
    return
end

local master, slave = libc_service.new()

slave.stat = [[
local real_stat, path = ...
local res, errno = real_stat(path)
print(res.dev)
print(res.ino)
print(bit.band(res.mode, 511))
print(res.nlink)
print(res.uid)
print(res.gid)
print(res.rdev)
print(res.size)
print(res.blksize)
print(res.blocks)
print(res.atim.sec, res.atim.nsec)
print(res.mtim.sec, res.mtim.nsec)
print(res.ctim.sec, res.ctim.nsec)
res.nlink = res.nlink + 1
return res, errno
]]

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
            dev = 1,
            ino = 2,
            mode = 3,
            type = 'regular',
            nlink = 4,
            uid = 5,
            gid = 6,
            rdev = 7,
            size = 8,
            blksize = 9,
            blocks = 10,
            atime = fs.clock.unix_epoch() + 1.000000001,
            mtime = fs.clock.unix_epoch() + 2.000000002,
            ctime = fs.clock.unix_epoch() + 3.000000003,
        }
    else
        master:use_slave_credentials()
    end
end end)
