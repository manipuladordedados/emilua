local future = require 'future'

local p, f = future.new()
spawn(function()
    this_fiber.yield()
    p:set_error('thing')
end):detach()
print(f:get())
