local future = require 'future'
local time = require 'time'

local p, f = future.new()

spawn(function()
    print(f:get())
end):detach()

spawn(function()
    print(f:get())
end):detach()

time.sleep(0.1)
p:set_value('twice')
