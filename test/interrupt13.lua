-- Read invalid cancellation_caught on detached finished fiber
fib = spawn(function() end)
this_fiber.yield()
fib:detach()
print(fib.cancellation_caught)
