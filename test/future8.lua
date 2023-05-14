local future = require 'future'

local p, f = future.new()
spawn(function()
    this_fiber.yield()
    p = nil
    collectgarbage('collect')
end):detach()
print(f:get())
