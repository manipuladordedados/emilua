-- Read invalid interruption_caught on some detached fiber
local println = require('println')

fib = spawn(function()
    this_fiber.yield()
end)
this_fiber.yield()
fib:detach()
println(tostring(fib.interruption_caught))
