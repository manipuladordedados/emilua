-- Read invalid cancellation_caught on some detached fiber
fib = spawn(function()
    this_fiber.yield()
end)
this_fiber.yield()
fib:detach()
print(fib.cancellation_caught)
