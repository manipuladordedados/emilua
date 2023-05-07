local recursive_mutex = require 'recursive_mutex'
local time = require 'time'

local m = recursive_mutex.new()

print('1', m:try_lock())
print('1b', m:try_lock())

local f = spawn(function()
    print('2', m:try_lock())
    m:lock()
    print('2b')
    print('2c', m:try_lock())
    m:unlock()
    m:unlock()
end)

time.sleep(0.1)
m:unlock()
print('1c')

time.sleep(0.1)
m:unlock()

f:join()
