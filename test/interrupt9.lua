-- Read invalid cancellation_caught on finished fiber
fib = spawn(function() end)
this_fiber.yield()
print(fib.cancellation_caught)
