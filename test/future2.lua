local future = require 'future'

local p, f = future.new()
spawn(function() this_fiber.yield(); p:set_value(1) end):detach()
print(f:get())
