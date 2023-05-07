local recursive_mutex = require 'recursive_mutex'
local time = require 'time'

local m = recursive_mutex.new()

m:lock()
m:lock()
print('1')

local f = spawn(function()
    m:lock()
    print('2')
    m:unlock()
end)

time.sleep(0.1)
m:unlock()
print('1b')

time.sleep(0.1)
m:unlock()

f:join()
