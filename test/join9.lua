local system = require 'system'

local f = spawn(function()
    this_fiber.yield()
    system.exit()
end)

f:join()
print('what')
