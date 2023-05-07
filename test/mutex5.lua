local mutex = require 'mutex'

local m = mutex.new()
print('1', m:try_lock())

spawn(function()
    print('2', m:try_lock())
end):join()

m:unlock()
