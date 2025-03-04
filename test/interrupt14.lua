-- Read invalid cancellation_caught on detached fiber who reacted to request
fib = spawn(function()
    this_fiber.yield()
end)
fib:cancel()
this_fiber.yield()
fib:detach()
print(fib.cancellation_caught)
