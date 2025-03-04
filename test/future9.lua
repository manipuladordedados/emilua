local future = require 'future'
local time = require 'time'

local p, f = future.new()

local fib = spawn(function()
    f:get()
    print('should never happen')
end)

time.sleep(0.1)
fib:cancel()
time.sleep(0.1)
p:set_value(44)

fib:join()
print(fib.cancellation_caught)
